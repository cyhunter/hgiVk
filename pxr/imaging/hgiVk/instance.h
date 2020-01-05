#ifndef PXR_IMAGING_HGIVK_INSTANCE_H
#define PXR_IMAGING_HGIVK_INSTANCE_H

#include "pxr/pxr.h"

#include "pxr/imaging/hgiVk/api.h"
#include "pxr/imaging/hgiVk/vulkan.h"

PXR_NAMESPACE_OPEN_SCOPE


class HgiVkInstance final {
public:
    HGIVK_API
    HgiVkInstance();

    HGIVK_API
    virtual ~HgiVkInstance();

    /// Returns the vulkan instance.
    HGIVK_API
    VkInstance GetVulkanInstance() const;

public:
    // Extra vulkan function pointers
    VkDebugReportCallbackEXT vkDebugCallback;
    PFN_vkCreateDebugReportCallbackEXT vkCreateDebugCallbackEXT;
    PFN_vkDestroyDebugReportCallbackEXT vkDestroyDebugCallbackEXT;

private:
    VkInstance _vkInstance;
};


PXR_NAMESPACE_CLOSE_SCOPE

#endif