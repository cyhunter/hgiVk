#include "pxr/base/tf/diagnostic.h"
#include "pxr/imaging/hgi/graphicsEncoderDesc.h"
#include "pxr/imaging/hgiVk/commandBuffer.h"
#include "pxr/imaging/hgiVk/commandPool.h"
#include "pxr/imaging/hgiVk/blitEncoder.h"
#include "pxr/imaging/hgiVk/device.h"
#include "pxr/imaging/hgiVk/diagnostic.h"
#include "pxr/imaging/hgiVk/graphicsEncoder.h"
#include "pxr/imaging/hgiVk/hgi.h"
#include "pxr/imaging/hgiVk/renderPass.h"
#include "pxr/imaging/hgiVk/texture.h"
#include "pxr/imaging/hgiVk/vulkan.h"


PXR_NAMESPACE_OPEN_SCOPE


HgiVkCommandBuffer::HgiVkCommandBuffer(
    HgiVkDevice* device,
    HgiVkCommandPool* commandPool,
    HgiVkCommandBufferUsage usage)
    : _device(device)
    , _commandPool(commandPool)
    , _usage(usage)
    , _isRecording(false)
    , _vkCommandBuffer(nullptr)
{
    //
    // Create command buffer
    //
    VkCommandBufferAllocateInfo allocateInfo =
        {VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};

    allocateInfo.commandBufferCount = 1;
    allocateInfo.commandPool = _commandPool->GetVulkanCommandPool();
    allocateInfo.level = _usage==HgiVkCommandBufferUsagePrimary ?
        VK_COMMAND_BUFFER_LEVEL_PRIMARY :
        VK_COMMAND_BUFFER_LEVEL_SECONDARY;

    TF_VERIFY(
        vkAllocateCommandBuffers(
            device->GetVulkanDevice(),
            &allocateInfo,
            &_vkCommandBuffer) == VK_SUCCESS
    );
}

HgiVkCommandBuffer::~HgiVkCommandBuffer()
{
    vkFreeCommandBuffers(
        _device->GetVulkanDevice(),
        _commandPool->GetVulkanCommandPool(),
        1, // command buffer cnt
        &_vkCommandBuffer);
}

void
HgiVkCommandBuffer::SetRenderPass(HgiVkRenderPass* rp)
{
    _vkInheritanceInfo =
        {VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_INFO};
    _vkInheritanceInfo.renderPass = rp->GetVulkanRenderPass();
    _vkInheritanceInfo.framebuffer = rp->GetVulkanFramebuffer();
}

void
HgiVkCommandBuffer::EndRecording()
{
    if (_isRecording) {
        TF_VERIFY(
            vkEndCommandBuffer(_vkCommandBuffer) == VK_SUCCESS
        );

        // Next frame this command buffer may be used by an entirely different
        // encoder, so clear the render pass info for secondary command buffers.
        _vkInheritanceInfo.renderPass = nullptr;
        _vkInheritanceInfo.framebuffer = nullptr;

        _isRecording = false;
    }
}

bool
HgiVkCommandBuffer::IsRecording() const
{
    return _isRecording;
}

VkCommandBuffer
HgiVkCommandBuffer::GetCommandBufferForRecoding()
{
    _BeginRecording();
    return _vkCommandBuffer;
}

VkCommandBuffer
HgiVkCommandBuffer::GetVulkanCommandBuffer() const
{
    return _vkCommandBuffer;
}

void
HgiVkCommandBuffer::_BeginRecording()
{
    if (!_isRecording) {

        VkCommandBufferBeginInfo beginInfo =
            {VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
        beginInfo.flags |= VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

        if (_usage==HgiVkCommandBufferUsageSecondaryRenderPass) {
            beginInfo.flags |= VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT;
            beginInfo.pInheritanceInfo = &_vkInheritanceInfo;
        }

        // Begin recording
        TF_VERIFY(
            vkBeginCommandBuffer(_vkCommandBuffer, &beginInfo) == VK_SUCCESS
        );

        _isRecording = true;
    }
}

PXR_NAMESPACE_CLOSE_SCOPE
