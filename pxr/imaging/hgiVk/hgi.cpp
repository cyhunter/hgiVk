#if defined(__linux__)
    #include <X11/Xlib.h>
#endif

#include "pxr/imaging/hgiVk/vulkan.h"

#define VMA_IMPLEMENTATION
    #include "vulkanMemoryAllocator/vk_mem_alloc.h"
#undef VMA_IMPLEMENTATION

#include "pxr/base/tf/diagnostic.h"
#include "pxr/base/work/threadLimits.h"

// XXX See if we can wrap tbb::global_control::active_value inside libWork
#define TBB_PREVIEW_GLOBAL_CONTROL 1
#include "tbb/global_control.h"

#include "pxr/imaging/hgiVk/blitEncoder.h"
#include "pxr/imaging/hgiVk/buffer.h"
#include "pxr/imaging/hgiVk/commandBuffer.h"
#include "pxr/imaging/hgiVk/commandBufferManager.h"
#include "pxr/imaging/hgiVk/device.h"
#include "pxr/imaging/hgiVk/hgi.h"
#include "pxr/imaging/hgiVk/instance.h"
#include "pxr/imaging/hgiVk/graphicsEncoder.h"
#include "pxr/imaging/hgiVk/parallelGraphicsEncoder.h"
#include "pxr/imaging/hgiVk/pipeline.h"
#include "pxr/imaging/hgiVk/resourceBindings.h"
#include "pxr/imaging/hgiVk/shaderFunction.h"
#include "pxr/imaging/hgiVk/shaderProgram.h"
#include "pxr/imaging/hgiVk/surface.h"
#include "pxr/imaging/hgiVk/swapchain.h"
#include "pxr/imaging/hgiVk/texture.h"


PXR_NAMESPACE_OPEN_SCOPE


// XXX using a static, globally available HgiVK allows us to share textures
// between the UI and hydra renderer, but feels like a bad idea once you have
// multiple hydra viewports...
HgiVk*
HgiVkGetHydraGraphicsInterface() {
    static HgiVk hgiVk;
    return &hgiVk;
}

static bool
_ValidateGraphicsEncoderDescriptor(
    HgiGraphicsEncoderDesc const& desc)
{
    if (!desc.HasAttachments()) {
        TF_WARN("Encoder descriptor not complete");
        return false;
    }

    const size_t maxColorAttachments = 8;
    if (!TF_VERIFY(desc.colorAttachments.size() <= maxColorAttachments,
            "Too many color attachments in descriptor")) {
        return false;
    }

    if (!TF_VERIFY(desc.width>0 && desc.height>0,
         "Graphics encoder descriptor widht and height cannot be 0")) {
        return false;
    }

    return true;
}

HgiVk::HgiVk()
    : _instance(nullptr)
    , _frameStarted(false)
{
    _instance = new HgiVkInstance();

    // Create "primary device" at front of vector.
    HgiVkDevice* dev = new HgiVkDevice(_instance, HgiVkPresentationType);
    TF_VERIFY(dev);
    _devices.push_back(dev);

    // Make sure HgiVk is ready to modify resources or record commands.
    _BeginFrame();
}

HgiVk::~HgiVk()
{
    DestroyHgiVk();
}

void
HgiVk::_BeginFrame()
{
    if (_frameStarted) return;
    _frameStarted = true;

    for (HgiVkDevice* d : _devices) {
        d->BeginFrame();
    }
}

void
HgiVk::EndFrame()
{
    // Submit the command buffer to GPU.
    for (HgiVkDevice* d : _devices) {
        d->EndFrame();
    }

    _frameStarted = false;

    // todo Defrag the AMD VulkanMemoryAllocator device->vmaAllocator
    //      You need to destroy/recreate vk buffers and update descriptorSets.
    //      See VMA header: defragmentation
    //      https://gpuopen-librariesandsdks.github.io/
    //      VulkanMemoryAllocator/html/defragmentation.html
    // XXX Keep in mind that persistent mapped buffers need to be remapped.
    // Additional notes:
    //      https://developer.nvidia.com/vulkan-memory-management
    //      https://ourmachinery.com/post/device-memory-management/

    // Hydra currently does not call BeginFrame() and even if it did during
    // Engine::Execute that would not be sufficient. HgiVk may be called via eg
    // sceneDelegate -> renderIndex -> DeletePrim -> HdRenderBuffer::Deallocate.
    // That call happens entirely outside of the Engine:Execute loop and we must
    // be ready to modify resources or record commands at any time.
    // So we prepare the next frame immediately after ending the last.
    // This is less efficient, because the gpu has less time to process the
    // command buffers we are about to re-use. By using a ringbuffer of size 3
    // we reduce the likelyhood of having to wait on the command buffers, but
    // introduces 2-frame latency for gpu to be fully up to date.
    _BeginFrame();
}

HgiGraphicsEncoderUniquePtr
HgiVk::CreateGraphicsEncoder(
    HgiGraphicsEncoderDesc const& desc)
{
    if (!_ValidateGraphicsEncoderDescriptor(desc)) return nullptr;

    HgiVkDevice* device = GetPrimaryDevice();
    HgiVkCommandBufferManager* cbm = device->GetCommandBufferManager();
    HgiVkCommandBuffer* cb = cbm->GetDrawCommandBuffer();
    HgiVkGraphicsEncoder* enc = new HgiVkGraphicsEncoder(device, cb, desc);

    return HgiGraphicsEncoderUniquePtr(enc);
}

HgiParallelGraphicsEncoderUniquePtr
HgiVk::CreateParallelGraphicsEncoder(
    HgiGraphicsEncoderDesc const& desc,
    HgiPipelineHandle pipeline)
{
    if (!_ValidateGraphicsEncoderDescriptor(desc)) return nullptr;

    HgiVkDevice* device = GetPrimaryDevice();
    HgiVkCommandBufferManager* cbm = device->GetCommandBufferManager();
    HgiVkCommandBuffer* cb = cbm->GetDrawCommandBuffer();
    HgiVkParallelGraphicsEncoder* enc =
        new HgiVkParallelGraphicsEncoder(device, cb, desc, pipeline);

    return HgiParallelGraphicsEncoderUniquePtr(enc);
}

HgiBlitEncoderUniquePtr
HgiVk::CreateBlitEncoder()
{
    HgiVkDevice* device = GetPrimaryDevice();
    HgiVkCommandBufferManager* cbm = device->GetCommandBufferManager();
    HgiVkCommandBuffer* cb = cbm->GetResourceCommandBuffer();
    return HgiBlitEncoderUniquePtr(new HgiVkBlitEncoder(device, cb));
}

HgiTextureHandle
HgiVk::CreateTexture(HgiTextureDesc const& desc)
{
    HgiVkDevice* device = GetPrimaryDevice();
    HgiVkCommandBufferManager* cbm = device->GetCommandBufferManager();
    HgiVkCommandBuffer* cb = cbm->GetResourceCommandBuffer();
    HgiVkTexture* tex = new HgiVkTexture(device, cb, desc);

    // If caller provided data to copy into this texture we create a staging
    // buffer to transfer this data from cpu to gpu. This allows the final gpu
    // texture to be of a 'faster' type while we do a non-blocking copy.

    if (desc.pixelData && desc.pixelsByteSize > 0) {
        // create staging buffer for cpu to gpu copy
        HgiBufferDesc stagingDesc;
        stagingDesc.usage = HgiBufferUsageTransferSrc;
        stagingDesc.byteSize = desc.pixelsByteSize;
        stagingDesc.data = desc.pixelData;

        HgiVkBuffer* stagingBuffer = new HgiVkBuffer(device, stagingDesc);

        // Record the copy
        tex->CopyTextureFrom(cb, *stagingBuffer);

        // Schedule destruction of staging buffer 3 frames from now.
        HgiVkObject stagingObject;
        stagingObject.buffer = stagingBuffer;
        stagingObject.type = HgiVkObjectTypeBuffer;
        device->DestroyObject(stagingObject);
    }

    return tex;
}

void
HgiVk::DestroyTexture(HgiTextureHandle* texHandle)
{
    if (TF_VERIFY(texHandle, "Invalid texture")) {
        HgiVkDevice* device = GetPrimaryDevice();
        HgiVkObject object;
        object.type = HgiVkObjectTypeTexture;
        if (object.texture = static_cast<HgiVkTexture*>(*texHandle)) {
            device->DestroyObject(object);
            *texHandle = nullptr;
        }
    }
}

HgiBufferHandle
HgiVk::CreateBuffer(HgiBufferDesc const& desc)
{
    HgiVkDevice* device = GetPrimaryDevice();
    HgiVkCommandBufferManager* cbm = device->GetCommandBufferManager();
    HgiVkBuffer* buffer = new HgiVkBuffer(device, desc);

    // If caller provided data to copy into this buffer we create a staging
    // buffer to transfer this data from cpu to gpu. This allows the final gpu
    // buffer to be of a 'faster' type while we do a non-blocking copy.

    if (desc.data && desc.byteSize > 0) {
        // create staging buffer for cpu to gpu copy
        HgiBufferDesc stagingDesc;
        stagingDesc.usage = HgiBufferUsageTransferSrc;
        stagingDesc.byteSize = desc.byteSize;
        stagingDesc.data = desc.data;

        HgiVkBuffer* stagingBuffer = new HgiVkBuffer(device, stagingDesc);

        // Record the copy
        HgiVkCommandBuffer* cb = cbm->GetResourceCommandBuffer();
        buffer->CopyBufferFrom(cb, *stagingBuffer);

        // Schedule destruction of staging buffer 3 frames from now.
        HgiVkObject stagingObject;
        stagingObject.buffer = stagingBuffer;
        stagingObject.type = HgiVkObjectTypeBuffer;
        device->DestroyObject(stagingObject);
    }

    return buffer;
}

void
HgiVk::DestroyBuffer(HgiBufferHandle* bufferHandle)
{
    if (TF_VERIFY(bufferHandle, "Invalid buffer")) {
        HgiVkDevice* device = GetPrimaryDevice();
        HgiVkObject object;
        object.type = HgiVkObjectTypeBuffer;
        if (object.buffer = static_cast<HgiVkBuffer*>(*bufferHandle)) {
            device->DestroyObject(object);
            *bufferHandle = nullptr;
        }
    }
}

HgiPipelineHandle
HgiVk::CreatePipeline(HgiPipelineDesc const& desc)
{
    HgiVkDevice* device = GetPrimaryDevice();
    return new HgiVkPipeline(device, desc);
}

void
HgiVk::DestroyPipeline(HgiPipelineHandle* pipeHandle)
{
    if (TF_VERIFY(pipeHandle, "Invalid pipeline")) {
        HgiVkDevice* device = GetPrimaryDevice();
        HgiVkObject object;
        object.type = HgiVkObjectTypePipeline;
        if (object.pipeline = static_cast<HgiVkPipeline*>(*pipeHandle)) {
            device->DestroyObject(object);
            *pipeHandle = nullptr;
        }
    }
}

HgiResourceBindingsHandle
HgiVk::CreateResourceBindings(HgiResourceBindingsDesc const& desc)
{
    HgiVkDevice* device = GetPrimaryDevice();
    return new HgiVkResourceBindings(device, desc);
}

void
HgiVk::DestroyResourceBindings(HgiResourceBindingsHandle* resHandle)
{
    if (TF_VERIFY(resHandle, "Invalid resource bindings")) {
        HgiVkDevice* device = GetPrimaryDevice();
        HgiVkObject object;
        object.type = HgiVkObjectTypeResourceBindings;
        if (object.resourceBindings =
                static_cast<HgiVkResourceBindings*>(*resHandle)) {
            device->DestroyObject(object);
            *resHandle = nullptr;
        }
    }
}

HgiShaderFunctionHandle
HgiVk::CreateShaderFunction(HgiShaderFunctionDesc const& desc)
{
    HgiVkDevice* device = GetPrimaryDevice();
    return new HgiVkShaderFunction(device, desc);
}

void
HgiVk::DestroyShaderFunction(HgiShaderFunctionHandle* shaderFunctionHandle)
{
    if (TF_VERIFY(shaderFunctionHandle, "Invalid shader function")) {
        HgiVkDevice* device = GetPrimaryDevice();
        HgiVkObject object;
        object.type = HgiVkObjectTypeShaderFunction;
        if (object.shaderFunction =
                static_cast<HgiVkShaderFunction*>(*shaderFunctionHandle)) {
            device->DestroyObject(object);
            *shaderFunctionHandle = nullptr;
        }
    }
}

HgiShaderProgramHandle
HgiVk::CreateShaderProgram(HgiShaderProgramDesc const& desc)
{
    return new HgiVkShaderProgram(desc);
}

void
HgiVk::DestroyShaderProgram(HgiShaderProgramHandle* shaderProgramHandle)
{
    if (TF_VERIFY(shaderProgramHandle, "Invalid shader program")) {
        HgiVkDevice* device = GetPrimaryDevice();
        HgiVkObject object;
        object.type = HgiVkObjectTypeShaderProgram;
        if (object.shaderProgram =
                static_cast<HgiVkShaderProgram*>(*shaderProgramHandle)) {
            device->DestroyObject(object);
            *shaderProgramHandle = nullptr;
        }
    }
}

void
HgiVk::GetMemoryInfo(size_t* used, size_t* unused)
{
    // XXX for now assume they want the primary device info.
    // But note we can have multiple _devices.
    HgiVkDevice* device = GetPrimaryDevice();
    device->GetDeviceMemoryInfo(used, unused);
}

HgiVkInstance*
HgiVk::GetVulkanInstance() const
{
    return _instance;
}

void*
HgiVk::GetVkInstance()
{
    return (void*) _instance->GetVulkanInstance();
}

HgiVkDevice*
HgiVk::GetPrimaryDevice() const
{
    if (_devices.empty()) return nullptr;
    return _devices.front();
}

HgiVkSurfaceHandle
HgiVk::CreateSurface(HgiVkSurfaceDesc const& desc)
{
    HgiVkDevice* device = GetPrimaryDevice();
    return new HgiVkSurface(_instance, device, desc);
}

void
HgiVk::DestroySurface(HgiVkSurfaceHandle* surfaceHandle)
{
    if (TF_VERIFY(surfaceHandle, "Invalid surface")) {
        HgiVkDevice* device = GetPrimaryDevice();
        HgiVkObject object;
        object.type = HgiVkObjectTypeSurface;
        if (object.surface = *surfaceHandle) {
            device->DestroyObject(object);
            *surfaceHandle = nullptr;
        }
    }
}

HgiVkSwapchainHandle
HgiVk::CreateSwapchain(HgiVkSurfaceHandle surfaceHandle)
{
    HgiVkDevice* device = GetPrimaryDevice();
    return new HgiVkSwapchain(device, surfaceHandle);
}

void
HgiVk::DestroySwapchain(HgiVkSwapchainHandle* swapchainHandle)
{
    if (TF_VERIFY(swapchainHandle, "Invalid swapchain")) {
        HgiVkDevice* device = GetPrimaryDevice();
        HgiVkObject object;
        object.type = HgiVkObjectTypeSwapchain;
        if (object.swapchain = *swapchainHandle) {
            device->DestroyObject(object);
            *swapchainHandle = nullptr;
        }
    }
}

void
HgiVk::DestroyHgiVk()
{
    for (HgiVkDevice* device : _devices) {
        delete device;
    }
    _devices.clear();

    delete _instance;
    _instance = nullptr;
}

unsigned int
HgiVk::GetThreadCount()
{
    // Hydra's RenderIndex uses WorkParallelForN to sync prims.

    // HgiVk parallel command recording relies on thread local storage vectors
    // that must be large enough so each thread has an index. So we must know
    // the maximum threads the WorkParallelForN may spawn.

    // While we can ask libWork what the limit is, it may not match with the
    // limit set inside TBB (we found libWork to often be 1 smaller than tbb).

    unsigned workMaxThreads = WorkGetConcurrencyLimit();

    unsigned tbbMaxThreads = (unsigned) tbb::global_control::active_value(
        tbb::global_control::max_allowed_parallelism);

    // Pick the largest of the two results.
    // +1 in case HdEngine::Execute runs on a thread and main thread also
    // uses HgiVk. (E.g. main thread composites hydra results in viewer)

    return std::max(tbbMaxThreads, workMaxThreads) + 1;
}

PXR_NAMESPACE_CLOSE_SCOPE
