#ifndef PXR_IMAGING_HGIVK_VULKAN_H
#define PXR_IMAGING_HGIVK_VULKAN_H

#include <vulkan/vulkan.h>
// vulkan.h includes X11/Xlib.h in Linux.
// Xlib.h globally defines Bool.
// This conflicts with things like: SdfValueTypeName Bool
#ifdef Bool
    #undef Bool
#endif

#include "vulkanMemoryAllocator/vk_mem_alloc.h"


#define HgiVkArraySize(_ARR)  ((int)(sizeof(_ARR)/sizeof(*_ARR)))

// Currently we use the default allocator
inline VkAllocationCallbacks*
HgiVkAllocator() {
    return nullptr;
}

#endif
