#include <cstring>

#include "pxr/base/tf/diagnostic.h"
#include "pxr/base/tf/envSetting.h"

#include "pxr/imaging/hgiVk/commandBuffer.h"
#include "pxr/imaging/hgiVk/device.h"
#include "pxr/imaging/hgiVk/diagnostic.h"
#include "pxr/imaging/hgiVk/instance.h"
#include "pxr/imaging/hgiVk/vulkan.h"


PXR_NAMESPACE_OPEN_SCOPE

#if defined(_DEBUG)
    TF_DEFINE_ENV_SETTING(HGIVK_DEBUG, 1, "Enable debugging for HgiVk");
#else
    TF_DEFINE_ENV_SETTING(HGIVK_DEBUG, 0, "Enable debugging for HgiVk");
#endif

// XXX Make HgiVkDebug class and move all function, including below in there.
// We may have multiple devices where one supports the marker extension and
// one does not.
static PFN_vkCmdDebugMarkerBeginEXT _vkCmdDebugMarkerBeginEXT = 0;
static PFN_vkCmdDebugMarkerEndEXT _vkCmdDebugMarkerEndEXT = 0;
static PFN_vkDebugMarkerSetObjectNameEXT _vkDebugMarkerSetObjectNameEXT = 0;

bool
HgiVkIsDebugEnabled()
{
    static bool _v = TfGetEnvSetting(HGIVK_DEBUG) == 1;
    return _v;
}

static VKAPI_ATTR VkBool32 VKAPI_CALL
_VulkanDebugCallback(
    VkDebugReportFlagsEXT flags,
    VkDebugReportObjectTypeEXT /*objectType*/,
    uint64_t /*object*/,
    size_t /*location*/,
    int32_t /*messageCode*/,
    const char* /*pLayerPrefix*/,
    const char* pMessage,
    void* /*pUserData*/)
{
    // XXX Validation layers don't correctly detect NonWriteable declarations
    // for storage buffers:
    // https://github.com/KhronosGroup/Vulkan-ValidationLayers/issues/73
    if (strstr(pMessage, "Shader requires vertexPipelineStoresAndAtomics but is"
                         " not enabled on the device")) {
        return VK_FALSE;
    }

    // XXX We are using VulkanMemoryAllocator and it allocates large blocks of
    // memory where buffers and images end up in the same memory block.
    // This may trigger a validation warning, that VMA itself also ignores.
    // https://github.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator
    // /blob/master/src/VulkanSample.cpp#L215
    if(strstr(pMessage, "Mapping an image with layout") != nullptr &&
       strstr(pMessage, "can result in undefined behavior if this memory is "
                        "used by the device") != nullptr) {
        return VK_FALSE;
    }

    // XXX We are using dedicated memory allocations
    // https://gpuopen-librariesandsdks.github.io/
    // VulkanMemoryAllocator/html/usage_patterns.html
    if(strstr(pMessage, "Binding memory to buffer") != nullptr &&
       strstr(pMessage, "but vkGetBufferMemoryRequirements() has not been "
                        "called on that buffer") != nullptr){
        return VK_FALSE;
    }

    const char* type =
        (flags & VK_DEBUG_REPORT_ERROR_BIT_EXT) ?
            "VULKAN_ERROR" :
        (flags & (VK_DEBUG_REPORT_WARNING_BIT_EXT |
                  VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT)) ?
            "VULKAN_WARNING" :
        "VULKAN_INFO";

    TF_WARN("%s: %s\n", type, pMessage);

    return VK_FALSE;
}

void
HgiVkCreateDebug(HgiVkInstance* instance)
{
    if (!HgiVkIsDebugEnabled()) return;

    VkInstance vkInstance = instance->GetVulkanInstance();

    instance->vkCreateDebugCallbackEXT = (PFN_vkCreateDebugReportCallbackEXT)
        vkGetInstanceProcAddr(vkInstance,"vkCreateDebugReportCallbackEXT");

    instance->vkDestroyDebugCallbackEXT = (PFN_vkDestroyDebugReportCallbackEXT)
        vkGetInstanceProcAddr(vkInstance,"vkDestroyDebugReportCallbackEXT");

    if (!TF_VERIFY(instance->vkCreateDebugCallbackEXT)) return;
    if (!TF_VERIFY(instance->vkDestroyDebugCallbackEXT)) return;

    VkDebugReportCallbackCreateInfoEXT debugReportCallbackCInfo;
    debugReportCallbackCInfo.sType =
        VK_STRUCTURE_TYPE_DEBUG_REPORT_CALLBACK_CREATE_INFO_EXT;

    debugReportCallbackCInfo.pNext = nullptr;
    debugReportCallbackCInfo.flags =
        VK_DEBUG_REPORT_WARNING_BIT_EXT |
        VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT |
        VK_DEBUG_REPORT_ERROR_BIT_EXT;
    debugReportCallbackCInfo.pfnCallback = _VulkanDebugCallback;
    debugReportCallbackCInfo.pUserData = nullptr;

    TF_VERIFY(
        instance->vkCreateDebugCallbackEXT(
            vkInstance,
            &debugReportCallbackCInfo,
            HgiVkAllocator(),
            &instance->vkDebugCallback) == VK_SUCCESS
    );
}

void
HgiVkDestroyDebug(HgiVkInstance* instance)
{
    if (!HgiVkIsDebugEnabled()) return;

    VkInstance vkInstance = instance->GetVulkanInstance();

    if (!TF_VERIFY(instance->vkDestroyDebugCallbackEXT)) return;

    instance->vkDestroyDebugCallbackEXT(
        vkInstance,
        instance->vkDebugCallback,
        HgiVkAllocator());
}

void
HgiVkInitializeDeviceDebug(HgiVkDevice* device)
{
    if (!HgiVkIsDebugEnabled()) return;
    if (!device->GetDeviceSupportDebugMarkers()) return;

    _vkCmdDebugMarkerBeginEXT = (PFN_vkCmdDebugMarkerBeginEXT)
        vkGetDeviceProcAddr(
            device->GetVulkanDevice(),
            "vkCmdDebugMarkerBeginEXT");

    _vkCmdDebugMarkerEndEXT = (PFN_vkCmdDebugMarkerEndEXT)
        vkGetDeviceProcAddr(
            device->GetVulkanDevice(),
            "vkCmdDebugMarkerEndEXT");

    _vkDebugMarkerSetObjectNameEXT = (PFN_vkDebugMarkerSetObjectNameEXT)
        vkGetDeviceProcAddr(
            device->GetVulkanDevice(),
            "vkDebugMarkerSetObjectNameEXT");
}

void
HgiVkBeginDebugMarker(
    HgiVkCommandBuffer* cb,
    const char* name)
{
    if (!HgiVkIsDebugEnabled() || !_vkCmdDebugMarkerBeginEXT) return;

    VkDebugMarkerMarkerInfoEXT markerBegin =
        {VK_STRUCTURE_TYPE_DEBUG_MARKER_MARKER_INFO_EXT};

    markerBegin.pMarkerName = name;
    const float color[4] = {1,1,0,1};
    markerBegin.color[0] = color[0];
    markerBegin.color[1] = color[1];
    markerBegin.color[2] = color[2];
    markerBegin.color[3] = color[3];
    _vkCmdDebugMarkerBeginEXT(
        cb->GetCommandBufferForRecoding(),
        &markerBegin);
}

void
HgiVkEndDebugMarker(HgiVkCommandBuffer* cb)
{
    if (!HgiVkIsDebugEnabled() || !_vkCmdDebugMarkerEndEXT) return;
    _vkCmdDebugMarkerEndEXT(cb->GetCommandBufferForRecoding());
}

void
HgiVkSetDebugName(
    HgiVkDevice* device,
    uint64_t vulkanObject,
    uint32_t objectType,
    const char* name)
{
    if (!HgiVkIsDebugEnabled() || !_vkDebugMarkerSetObjectNameEXT) return;

    VkDebugMarkerObjectNameInfoEXT debugInfo =
        {VK_STRUCTURE_TYPE_DEBUG_MARKER_OBJECT_TAG_INFO_EXT};
    debugInfo.object = vulkanObject;
    debugInfo.objectType = VkDebugReportObjectTypeEXT(objectType);
    debugInfo.pObjectName = name;
    _vkDebugMarkerSetObjectNameEXT(device->GetVulkanDevice(), &debugInfo);
}

PXR_NAMESPACE_CLOSE_SCOPE

