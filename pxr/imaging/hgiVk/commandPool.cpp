#include "pxr/base/tf/diagnostic.h"
#include "pxr/imaging/hgiVk/commandPool.h"
#include "pxr/imaging/hgiVk/device.h"


PXR_NAMESPACE_OPEN_SCOPE


HgiVkCommandPool::HgiVkCommandPool(HgiVkDevice* device)
    : _device(device)
    , _vkCommandPool(nullptr)
{
    VkCommandPoolCreateInfo poolCreateInfo =
        {VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
    poolCreateInfo.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
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

PXR_NAMESPACE_CLOSE_SCOPE
