#include "pxr/base/tf/diagnostic.h"
#include "pxr/base/tf/envSetting.h"

#include "pxr/imaging/hgiVk/device.h"
#include "pxr/imaging/hgiVk/diagnostic.h"
#include "pxr/imaging/hgiVk/hgi.h"
#include "pxr/imaging/hgiVk/instance.h"
#include "pxr/imaging/hgiVk/renderPass.h"

PXR_NAMESPACE_OPEN_SCOPE


static uint32_t
_GetGraphicsFamilyIndex(VkPhysicalDevice physicalDevice)
{
    uint32_t queueCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueCount, 0);

    std::vector<VkQueueFamilyProperties> queues(queueCount);
    vkGetPhysicalDeviceQueueFamilyProperties(
        physicalDevice,
        &queueCount,
        queues.data());

    for (uint32_t i = 0; i < queueCount; i++)
        if (queues[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
            return i;
        }

    return VK_QUEUE_FAMILY_IGNORED;
}

static bool
_SupportsPresentation(
    VkPhysicalDevice physicalDevice,
    uint32_t familyIndex)
{
    #if defined(VK_USE_PLATFORM_WIN32_KHR)
        return vkGetPhysicalDeviceWin32PresentationSupportKHR(
                    physicalDevice, familyIndex);
    #elif defined(VK_USE_PLATFORM_XLIB_KHR)
        Display* dsp = XOpenDisplay(nullptr);
        VisualID visualID = XVisualIDFromVisual(
            DefaultVisual(dsp, DefaultScreen(dsp)));
        return vkGetPhysicalDeviceXlibPresentationSupportKHR(
                    physicalDevice, familyIndex, dsp, visualID);
    #elif defined(VK_USE_PLATFORM_MACOS_MVK)
        // If we need to we can query metal features, but presentation is
        // currently always supported.
        //     MVKPhysicalDeviceMetalFeatures features;
        //     size_t featuresSize = 0;
        //     vkGetPhysicalDeviceMetalFeaturesMVK(
        //          physicalDevice, &features, &featuresSize);
        return true;
    #else
        #error Unsupported Platform
        return true;
    #endif
}

HgiVkDevice::HgiVkDevice(
    HgiVkInstance* instance,
    HgiVkDeviceSettings deviceType)
    : _vmaAllocator(nullptr)
    , _vkPhysicalDevice(nullptr)
    , _vkDevice(nullptr)
    , _vkQueueFamilyIndex(0)
    , _vkQueue(nullptr)
    , _vkPipelineCache(nullptr)
    , _supportsDebugMarkers(false)
    , _frame(~0ull)
    , _frameStarted(false)
    , _ringBufferIndex(-1)
{
    //
    // Determine physical device
    //

    const uint32_t maxDevices = 64;
    VkPhysicalDevice physicalDevices[maxDevices];
    uint32_t physicalDeviceCount = maxDevices;
    TF_VERIFY(
        vkEnumeratePhysicalDevices(
            instance->GetVulkanInstance(),
            &physicalDeviceCount,
            physicalDevices) == VK_SUCCESS
    );

    VkPhysicalDevice discrete = nullptr;
    VkPhysicalDevice fallback = nullptr;
    uint32_t familyIndex = 0;

    for (uint32_t i = 0; i < physicalDeviceCount; i++) {
        VkPhysicalDeviceProperties props;
        vkGetPhysicalDeviceProperties(physicalDevices[i], &props);

        familyIndex = _GetGraphicsFamilyIndex(physicalDevices[i]);
        if (familyIndex == VK_QUEUE_FAMILY_IGNORED) continue;

        if (deviceType == HgiVkPresentationType) {
            if (!_SupportsPresentation(physicalDevices[i], familyIndex)) {
                continue;
            }
        } else {
            TF_CODING_ERROR("VULKAN_ERROR: Unknown device type requested");
        }

        if (props.apiVersion < VK_API_VERSION_1_0) continue;

        if (!discrete && props.deviceType==VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU){
            discrete = physicalDevices[i];
        }

        if (!fallback) {
            fallback = physicalDevices[i];
        }
    }

    _vkPhysicalDevice = discrete ? discrete : fallback;

    if (_vkPhysicalDevice) {
        vkGetPhysicalDeviceProperties(
            _vkPhysicalDevice,
            &_vkDeviceProperties);

        vkGetPhysicalDeviceFeatures(
            _vkPhysicalDevice,
            &_vkDeviceFeatures);

        vkGetPhysicalDeviceMemoryProperties(
            _vkPhysicalDevice,
            &_vkMemoryProperties);

        #if defined(_DEBUG)
            TF_WARN("Selected GPU %s", _vkDeviceProperties.deviceName);
        #endif
    } else {
        TF_CODING_ERROR("VULKAN_ERROR: Unable to determine physical device");
        return;
    }

    //
    // Query supported extensions for device
    //

    uint32_t extensionCount = 0;
	TF_VERIFY(
        vkEnumerateDeviceExtensionProperties(
            _vkPhysicalDevice,
            nullptr,
            &extensionCount,
            nullptr) == VK_SUCCESS
    );

    _extensions.resize(extensionCount);

    TF_VERIFY(
        vkEnumerateDeviceExtensionProperties(
            _vkPhysicalDevice,
            nullptr,
            &extensionCount,
            _extensions.data()) == VK_SUCCESS
    );

    //
    // Create Device
    //

    VkDeviceQueueCreateInfo queueInfo =
        {VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO};
    float queuePriorities[] = {1.0f};
    queueInfo.queueFamilyIndex = familyIndex;
    queueInfo.queueCount = 1;
    queueInfo.pQueuePriorities = queuePriorities;

    std::vector<const char*> extensions = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME
    };

    _supportsDebugMarkers =
        _IsSupportedExtension(VK_EXT_DEBUG_MARKER_EXTENSION_NAME);

    if (_supportsDebugMarkers && HgiVkIsDebugEnabled()) {
        extensions.push_back(VK_EXT_DEBUG_MARKER_EXTENSION_NAME);
    }

    // Allow certain buffers/images to have dedicated memory allocations to
    // improve performance on some GPUs.
    // https://gpuopen-librariesandsdks.github.io/
    // VulkanMemoryAllocator/html/usage_patterns.html
    bool dedicatedAllocations = false;
    if (_IsSupportedExtension(VK_KHR_GET_MEMORY_REQUIREMENTS_2_EXTENSION_NAME) &&
        _IsSupportedExtension(VK_KHR_DEDICATED_ALLOCATION_EXTENSION_NAME)) {
        dedicatedAllocations = true;
        extensions.push_back(VK_KHR_GET_MEMORY_REQUIREMENTS_2_EXTENSION_NAME);
        extensions.push_back(VK_KHR_DEDICATED_ALLOCATION_EXTENSION_NAME);
    }

    bool supportsMemExtension = false;
    if (_IsSupportedExtension(VK_EXT_MEMORY_BUDGET_EXTENSION_NAME)) {
        supportsMemExtension = true;
        extensions.push_back(VK_EXT_MEMORY_BUDGET_EXTENSION_NAME);
    }

    // This extension is needed to allow the viewport to be flipped in Y so that
    // shaders and vertex data can remain the same between opengl and vulkan.
    // See GraphicsEncoder::SetViewport. This extension is core as of 1.1.
    extensions.push_back(VK_KHR_MAINTENANCE1_EXTENSION_NAME);

    VkPhysicalDeviceFeatures2 features =
        {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2};

    features.features.samplerAnisotropy =
        _vkDeviceFeatures.samplerAnisotropy;
    features.features.shaderSampledImageArrayDynamicIndexing =
        _vkDeviceFeatures.shaderSampledImageArrayDynamicIndexing;
    features.features.shaderStorageImageArrayDynamicIndexing =
        _vkDeviceFeatures.shaderStorageImageArrayDynamicIndexing;
    features.features.sampleRateShading =
        _vkDeviceFeatures.sampleRateShading;

    VkDeviceCreateInfo createInfo = {VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO};
    createInfo.queueCreateInfoCount = 1;
    createInfo.pQueueCreateInfos = &queueInfo;
    createInfo.ppEnabledExtensionNames = extensions.data();
    createInfo.enabledExtensionCount = (uint32_t)extensions.size();
    createInfo.pNext = &features;

    TF_VERIFY(
        vkCreateDevice(
            _vkPhysicalDevice,
            &createInfo,
            HgiVkAllocator(),
            &_vkDevice) == VK_SUCCESS
    );

    HgiVkInitializeDeviceDebug(this);

    //
    // Memory allocator
    //

    VmaAllocatorCreateInfo allocatorInfo = {};
    allocatorInfo.instance = instance->GetVulkanInstance();
    allocatorInfo.physicalDevice = _vkPhysicalDevice;
    allocatorInfo.device = _vkDevice;
    if (dedicatedAllocations) {
        allocatorInfo.flags |=VMA_ALLOCATOR_CREATE_KHR_DEDICATED_ALLOCATION_BIT;
    }

    if (supportsMemExtension) {
        allocatorInfo.flags |= VMA_ALLOCATOR_CREATE_EXT_MEMORY_BUDGET_BIT;
    }

    TF_VERIFY(
        vmaCreateAllocator(&allocatorInfo, &_vmaAllocator) == VK_SUCCESS
    );

    //
    // Device Queue
    //

    const uint32_t queueIndex = 0;
    vkGetDeviceQueue(_vkDevice, _vkQueueFamilyIndex, queueIndex, &_vkQueue);

    // Create the ring-buffer render frames.
    for (size_t i=0; i< HgiVkRingBufferSize; i++) {
        _frames.push_back(new HgiVkRenderFrame(this));
    }
}

HgiVkDevice::~HgiVkDevice()
{
    // Make sure device is done consuming all frames before destroying objects.
    TF_VERIFY(vkDeviceWaitIdle(_vkDevice) == VK_SUCCESS);

    // Destroy render passes in cache before clearing the frame, because the
    // to-be-destroyed render passes will go into the frame garbage collecter.
    // Then on clearing the frames, the garbage collector destroys them.
    _renderPassPipelineCache.Clear();

    // Destroy vulkan objects in the frames before destroying this device.
    for (HgiVkRenderFrame* frame : _frames) {
        delete frame;
    }
    _frames.clear();

    vmaDestroyAllocator(_vmaAllocator);
    vkDestroyDevice(_vkDevice, HgiVkAllocator());
}

void
HgiVkDevice::BeginFrame()
{
    if (_frameStarted) return;
    _frameStarted = true;

    // Increment the frame counter.
    _frame++;

    // Each new frame we reset what command buffers are used and switch to the
    // next index in the 'ring buffer'. This ensures last frames command buffers
    // are fully consumed by the gpu before we re-use them.
    _ringBufferIndex = (_ringBufferIndex+1) % HgiVkRingBufferSize;
    HgiVkRenderFrame* frame = _frames[_ringBufferIndex];
    frame->BeginFrame(_frame);

    // Ensure render pass and pipeline cache is configured for a new frame.
    _renderPassPipelineCache.BeginFrame(_frame);
}

void
HgiVkDevice::EndFrame()
{
    HgiVkRenderFrame* frame = _frames[_ringBufferIndex];
    frame->EndFrame();

    // Store all thread_local, newly created render passes.
    _renderPassPipelineCache.EndFrame();

    _frameStarted = false;
}

HgiVkCommandBufferManager*
HgiVkDevice::GetCommandBufferManager()
{
    HgiVkRenderFrame* frame = _frames[_ringBufferIndex];
    return frame->GetCommandBufferManager();
}

void
HgiVkDevice::SubmitToQueue(
    std::vector<VkSubmitInfo> const& submitInfos,
    VkFence fence)
{
    /* MULTI-THREAD CALL*/

    if (submitInfos.empty()) return;

    // The vkQueue must be externally synchronized. We can have another
    // thread submitting to the queue, such as a blitEncoder copy cmd or a
    // compute command that must be immediately submitted for CPU read back.

    std::lock_guard<std::mutex> lock(_queuelock);

    // Commit provided command buffers to queue.
    // Record and submission order does not guarantee execution order.
    // Vulkan docs: "Execution Model" and "Implicit Synchronization Guarantees".

    TF_VERIFY(
        vkQueueSubmit(
            _vkQueue,
            (uint32_t) submitInfos.size(),
            submitInfos.data(),
            fence) == VK_SUCCESS
    );
}

VkDevice
HgiVkDevice::GetVulkanDevice() const
{
    return _vkDevice;
}

VkPhysicalDevice
HgiVkDevice::GetVulkanPhysicalDevice() const
{
    return _vkPhysicalDevice;
}

VkPhysicalDeviceProperties
HgiVkDevice::GetVulkanPhysicalDeviceProperties() const
{
    return _vkDeviceProperties;
}

VkPhysicalDeviceFeatures
HgiVkDevice::GetVulkanPhysicalDeviceFeatures() const
{
    return _vkDeviceFeatures;
}

VmaAllocator
HgiVkDevice::GetVulkanMemoryAllocator() const
{
    return _vmaAllocator;
}

HgiVkRenderPass*
HgiVkDevice::AcquireRenderPass(HgiGraphicsEncoderDesc const& desc)
{
    return _renderPassPipelineCache.AcquireRenderPass(this, desc);
}

void
HgiVkDevice::ReleaseRenderPass(HgiVkRenderPass* rp)
{
    rp->ReleaseRenderPass();
}

VkQueue
HgiVkDevice::GetVulkanDeviceQueue() const
{
    return _vkQueue;
}

uint32_t
HgiVkDevice::GetVulkanDeviceQueueFamilyIndex() const
{
    return _vkQueueFamilyIndex;
}

VkPipelineCache
HgiVkDevice::GetVulkanPipelineCache() const
{
    return _vkPipelineCache;
}

HgiVkShaderCompiler*
HgiVkDevice::GetShaderCompiler()
{
    return &_shaderCompiler;
}

void
HgiVkDevice::DestroyObject(HgiVkObject const& object)
{
    HgiVkRenderFrame* frame = _frames[_ringBufferIndex];
    frame->GetGarbageCollector()->ScheduleObjectDestruction(object);
}

void
HgiVkDevice::WaitForIdle()
{
    TF_VERIFY(
        vkDeviceWaitIdle(_vkDevice) == VK_SUCCESS
    );
}

uint64_t
HgiVkDevice::GetCurrentFrame() const
{
    return _frame;
}

void
HgiVkDevice::GetDeviceMemoryInfo(size_t* used, size_t* unused) const
{
    VmaBudget budget[VK_MAX_MEMORY_HEAPS] = {};
    vmaGetBudget(_vmaAllocator, budget);

    size_t memUsed = 0;
    size_t memTotal = 0;

    for (size_t i=0; i<VK_MAX_MEMORY_HEAPS; i++) {
        memUsed += budget[i].usage;
        memTotal += budget[i].budget;
    }

    if (used) *used = memUsed;
    if (unused) *unused = memTotal - memUsed;
}

bool
HgiVkDevice::GetDeviceSupportDebugMarkers() const
{
    return _supportsDebugMarkers;
}

bool
HgiVkDevice::_IsSupportedExtension(const char* extensionName) const
{
    for (VkExtensionProperties const& ext : _extensions) {
        if (!strcmp(extensionName, ext.extensionName)) {
            return true;
        }
    }

    return false;
}


PXR_NAMESPACE_CLOSE_SCOPE
