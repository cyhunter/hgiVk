#include "pxr/base/tf/diagnostic.h"
#include "pxr/imaging/hgiVk/commandPool.h"
#include "pxr/imaging/hgiVk/device.h"
#include "pxr/imaging/hgiVk/diagnostic.h"


PXR_NAMESPACE_OPEN_SCOPE


HgiVkCommandPool::HgiVkCommandPool(HgiVkDevice* device)
    : _device(device)
    , _vkCommandPool(nullptr)
{
    VkCommandPoolCreateInfo poolCreateInfo =
        {VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
    poolCreateInfo.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;

    // XXX If Graphics and Compute were to come from different queue families we
    // would need to use a different commandpool/buffer for gfx and compute.
    poolCreateInfo.queueFamilyIndex = device->GetVulkanDeviceQueueFamilyIndex();

    TF_VERIFY(
        vkCreateCommandPool(
            device->GetVulkanDevice(),
            &poolCreateInfo,
            HgiVkAllocator(),
            &_vkCommandPool) == VK_SUCCESS
    );
}

HgiVkCommandPool::~HgiVkCommandPool()
{
    vkDestroyCommandPool(
        _device->GetVulkanDevice(),
        _vkCommandPool,
        HgiVkAllocator());
}

void
HgiVkCommandPool::ResetCommandPool()
{
    TF_VERIFY(
        vkResetCommandPool(
            _device->GetVulkanDevice(),
            _vkCommandPool,
            0 /*flags*/) == VK_SUCCESS
    );
}

VkCommandPool
HgiVkCommandPool::GetVulkanCommandPool() const
{
    return _vkCommandPool;
}

void
HgiVkCommandPool::SetDebugName(std::string const& name)
{
    std::string debugLabel = "Command Pool " + name;
    HgiVkSetDebugName(
        _device,
        (uint64_t)_vkCommandPool,
        VK_DEBUG_REPORT_OBJECT_TYPE_COMMAND_POOL_EXT,
        debugLabel.c_str());
}

PXR_NAMESPACE_CLOSE_SCOPE
