#include "pxr/imaging/hgi/graphicsEncoderDesc.h"

#include "pxr/imaging/hgiVk/commandBuffer.h"
#include "pxr/imaging/hgiVk/commandBufferManager.h"
#include "pxr/imaging/hgiVk/device.h"
#include "pxr/imaging/hgiVk/diagnostic.h"
#include "pxr/imaging/hgiVk/graphicsEncoder.h"
#include "pxr/imaging/hgiVk/hgi.h"
#include "pxr/imaging/hgiVk/parallelGraphicsEncoder.h"
#include "pxr/imaging/hgiVk/pipeline.h"
#include "pxr/imaging/hgiVk/renderPass.h"
#include "pxr/imaging/hgiVk/vulkan.h"

PXR_NAMESPACE_OPEN_SCOPE


HgiVkParallelGraphicsEncoder::HgiVkParallelGraphicsEncoder(
    const char* debugName,
    HgiVkDevice* device,
    HgiVkCommandBuffer* primaryCB,
    HgiGraphicsEncoderDesc const& desc,
    HgiPipelineHandle pipeline)
    : HgiParallelGraphicsEncoder(debugName)
    , _device(device)
    , _primaryCommandBuffer(primaryCB)
    , _renderPass(nullptr)
    , _isRecording(true)
    , _isDebugging(debugName!=nullptr)
    , _cmdBufBlockId(0)
{
    if (_isDebugging) {
        _primaryCommandBuffer->PushTimeQuery(debugName);
        HgiVkBeginDebugMarker(_primaryCommandBuffer, debugName);
    }

    // Make sure there are enough secondary commmand buffer for this parallel
    // encoder to use during CreateGraphicsEncoder().
    HgiVkCommandBufferManager* cbm = _device->GetCommandBufferManager();
    _cmdBufBlockId = cbm->ReserveSecondaryDrawBuffersForParallelEncoder();

    // In vulkan the render pass must begin and end in one primary command
    // buffer. So we begin it here in the parallel encoder instead of in the
    // individual graphics encoders that are used in the threads.
    // This will ensure the load op for each attachment happens once.
    _renderPass = device->AcquireRenderPass(desc);
    _renderPass->BeginRenderPass(_primaryCommandBuffer, /*use secondary*/ true);

    // Client will call BindPipeline on each graphics encoder, but we must make
    // sure that the right vkPipeline is our internal 'renderpass-pipeline'
    // cache that lives inside HgiVkPipeline. Normally this vkPipeline is
    // created on-the-fly during BindPipeline, but that call is not threadsafe.
    // By calling it here, we make sure the vkPipeline is created and inside
    // the cache for when the parallel encoders try to acquire it.
    if (HgiVkPipeline* p = static_cast<HgiVkPipeline*>(pipeline)) {
        p->AcquirePipeline(_renderPass);
    }
}

HgiVkParallelGraphicsEncoder::~HgiVkParallelGraphicsEncoder()
{
    if (_isRecording) {
        TF_WARN("Parallel Gfx Encoder: [%x] is missing an EndEncodig() call.");
        EndEncoding();
    }
}

void
HgiVkParallelGraphicsEncoder::EndEncoding()
{
    // Record secondary cmd bufs into primary cmd buf
    HgiVkCommandBufferManager* cbm = _device->GetCommandBufferManager();
    cbm->ExecuteSecondaryCommandBuffers(_cmdBufBlockId, _primaryCommandBuffer);

    // End the render pass (perform store ops)
    _renderPass->EndRenderPass(_primaryCommandBuffer);
    _device->ReleaseRenderPass(_renderPass);

    if (_isDebugging) {
        HgiVkEndDebugMarker(_primaryCommandBuffer);
        _primaryCommandBuffer->PopTimeQuery();
    }

    // No more recording allowed
    _primaryCommandBuffer = nullptr;
    _isRecording = false;
}

HgiGraphicsEncoderUniquePtr
HgiVkParallelGraphicsEncoder::CreateGraphicsEncoder()
{
    /* MULTI-THREAD CALL*/

    if (!TF_VERIFY(_isRecording, "Parallel recording ended")) {
        return nullptr;
    }

    // Get thread_local secondary command buffer
    HgiVkCommandBufferManager* cbm = _device->GetCommandBufferManager();
    HgiVkCommandBuffer* cb = cbm->GetSecondaryDrawCommandBuffer(_cmdBufBlockId);

    // The secondary command buffer needs to know what render pass it is part of
    cb->SetRenderPass(_renderPass);

    // Create the graphics encoder passing it our already started render pass.
    HgiVkGraphicsEncoder* enc = new HgiVkGraphicsEncoder(
        _device, cb, _renderPass);

    return HgiGraphicsEncoderUniquePtr(enc);
}


PXR_NAMESPACE_CLOSE_SCOPE
