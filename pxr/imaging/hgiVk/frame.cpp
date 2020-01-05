#include "pxr/base/tf/diagnostic.h"
#include "pxr/imaging/hgiVk/device.h"
#include "pxr/imaging/hgiVk/frame.h"


PXR_NAMESPACE_OPEN_SCOPE


HgiVkRenderFrame::HgiVkRenderFrame(HgiVkDevice* device)
    : _device(device)
    , _commandBufferManager(device)
    , _vkFence(nullptr)
{
    // Create fence (for cpu synchronization)
    VkFenceCreateInfo fenceInfo = {VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
    fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    TF_VERIFY(
        vkCreateFence(
            _device->GetVulkanDevice(),
            &fenceInfo,
            HgiVkAllocator(),
            &_vkFence) == VK_SUCCESS
    );
}

HgiVkRenderFrame::~HgiVkRenderFrame()
{
    vkDestroyFence(_device->GetVulkanDevice(), _vkFence, HgiVkAllocator());
}

void
HgiVkRenderFrame::BeginFrame(uint64_t frame)
{
    // Wait until the command buffers we are about to re-use have been
    // consumed by GPU. Which may result in no wait at all since we use a
    // ring-buffer of cmd buffers. Reset fence so it may be re-used.
    TF_VERIFY(
        vkWaitForFences(
            _device->GetVulkanDevice(),
            1,
            &_vkFence,
            VK_TRUE,
            100000000000) == VK_SUCCESS
    );

    TF_VERIFY(
        vkResetFences(
            _device->GetVulkanDevice(),
            1,
            &_vkFence)  == VK_SUCCESS
    );

    // Above we waited to ensure the command buffers are no longer in flight.
    // This means we can now delete all objects that were put in the garbage
    // collector several frames ago.
    _garbageCollector.DestroyGarbage(frame);

    // Command buffer manager should reset command pools etc
    _commandBufferManager.BeginFrame(frame);
}

void
HgiVkRenderFrame::EndFrame()
{
    _commandBufferManager.EndFrame(_vkFence);
}

HgiVkGarbageCollector*
HgiVkRenderFrame::GetGarbageCollector()
{
    return &_garbageCollector;
}

HgiVkCommandBufferManager*
HgiVkRenderFrame::GetCommandBufferManager()
{
    return &_commandBufferManager;
}

PXR_NAMESPACE_CLOSE_SCOPE
