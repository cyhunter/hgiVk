#ifndef PXR_IMAGING_HGIVK_COMMAND_POOL_H
#define PXR_IMAGING_HGIVK_COMMAND_POOL_H

#include <vector>

#include "pxr/pxr.h"
#include "pxr/imaging/hgi/graphicsEncoderDesc.h"
#include "pxr/imaging/hgiVk/api.h"
#include "pxr/imaging/hgiVk/vulkan.h"

PXR_NAMESPACE_OPEN_SCOPE

class HgiVkDevice;


/// \class HgiVkCommandPool
///
/// Wrapper for vulkan command pool
///
class HgiVkCommandPool final
{
public:
    HGIVK_API
    HgiVkCommandPool(HgiVkDevice* device);

    HGIVK_API
    virtual ~HgiVkCommandPool();

    /// Reset the command pool.
    HGIVK_API
    void ResetCommandPool();

    /// Returns the vulkan command pool.
    HGIVK_API
    VkCommandPool GetVulkanCommandPool() const;

private:
    HgiVkCommandPool() = delete;
    HgiVkCommandPool & operator= (const HgiVkCommandPool&) = delete;
    HgiVkCommandPool(const HgiVkCommandPool&) = delete;

private:
    HgiVkDevice* _device;
    VkCommandPool _vkCommandPool;
};

typedef std::vector<HgiVkCommandPool*> HgiVkCommandPoolVector;


PXR_NAMESPACE_CLOSE_SCOPE

#endif
