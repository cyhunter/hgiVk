#ifndef PXR_IMAGING_HGIVK_DIAGNOSTIC_H
#define PXR_IMAGING_HGIVK_DIAGNOSTIC_H

#include <stdint.h>

#include "pxr/pxr.h"
#include "pxr/imaging/hgiVk/api.h"

PXR_NAMESPACE_OPEN_SCOPE

class HgiVkInstance;
class HgiVkDevice;
class HgiVkCommandBuffer;


/// Returns true if debugging is enabled (HGIVK_DEBUG=1)
HGIVK_API
bool HgiVkIsDebugEnabled();

/// Setup vulkan debug callbacks
HGIVK_API
void HgiVkCreateDebug(HgiVkInstance* instance);

/// Tear down vulkan debug callbacks
HGIVK_API
void HgiVkDestroyDebug(HgiVkInstance* instance);

/// Setup vulkan device debug function ptrs
HGIVK_API
void HgiVkInitializeDeviceDebug(HgiVkDevice* device);

/// Push a debug marker
HGIVK_API
void HgiVkBeginDebugMarker(
    HgiVkCommandBuffer* cmdBuf,
    const char* name);

/// Pop a debug marker
HGIVK_API
void HgiVkEndDebugMarker(
    HgiVkCommandBuffer* cb);

/// Add a debug name to a vulkan object
HGIVK_API
void HgiVkSetDebugName(
    HgiVkDevice* device,
    uint64_t vulkanObject,
    uint32_t /*VkDebugReportObjectTypeEXT*/ objectType,
    const char* name);

PXR_NAMESPACE_CLOSE_SCOPE

#endif
