#include <vector>

#include "pxr/base/tf/diagnostic.h"

#include "pxr/imaging/hgiVk/diagnostic.h"
#include "pxr/imaging/hgiVk/instance.h"


PXR_NAMESPACE_OPEN_SCOPE


HgiVkInstance::HgiVkInstance()
    : vkDebugCallback(nullptr)
    , vkCreateDebugCallbackEXT(nullptr)
    , vkDestroyDebugCallbackEXT(nullptr)
    , _vkInstance(nullptr)
{
    VkApplicationInfo appInfo = {VK_STRUCTURE_TYPE_APPLICATION_INFO};
    appInfo.apiVersion = VK_API_VERSION_1_0;

    VkInstanceCreateInfo createInfo = {VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};
    createInfo.pApplicationInfo = &appInfo;

    std::vector<const char*> extensions = {
        VK_KHR_SURFACE_EXTENSION_NAME,
        #if defined(VK_USE_PLATFORM_WIN32_KHR)
            VK_KHR_WIN32_SURFACE_EXTENSION_NAME,
        #elif defined(VK_USE_PLATFORM_XLIB_KHR)
            VK_KHR_XLIB_SURFACE_EXTENSION_NAME,
        #elif defined(VK_USE_PLATFORM_MACOS_MVK)
            VK_MVK_MACOS_SURFACE_EXTENSION_NAME,
        #else
            #error Unsupported Platform
        #endif

        VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME,
    };

    if (HgiVkIsDebugEnabled()) {
        extensions.push_back(VK_EXT_DEBUG_REPORT_EXTENSION_NAME);

        const char* debugLayers[] = {
            "VK_LAYER_LUNARG_standard_validation"
        };
        createInfo.ppEnabledLayerNames = debugLayers;
        createInfo.enabledLayerCount = HgiVkArraySize(debugLayers);
    }

    createInfo.ppEnabledExtensionNames = extensions.data();
    createInfo.enabledExtensionCount = (uint32_t) extensions.size();

    TF_VERIFY(
        vkCreateInstance(
            &createInfo,
            HgiVkAllocator(),
            &_vkInstance) == VK_SUCCESS
    );

    HgiVkCreateDebug(this);
}

HgiVkInstance::~HgiVkInstance()
{
    HgiVkDestroyDebug(this);
    vkDestroyInstance(_vkInstance, HgiVkAllocator());
}

VkInstance
HgiVkInstance::GetVulkanInstance() const
{
    return _vkInstance;
}


PXR_NAMESPACE_CLOSE_SCOPE
