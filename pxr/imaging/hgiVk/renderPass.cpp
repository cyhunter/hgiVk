#include "pxr/imaging/hgi/graphicsEncoderDesc.h"

#include "pxr/imaging/hgiVk/commandBuffer.h"
#include "pxr/imaging/hgiVk/conversions.h"
#include "pxr/imaging/hgiVk/device.h"
#include "pxr/imaging/hgiVk/renderPass.h"
#include "pxr/imaging/hgiVk/texture.h"
#include "pxr/imaging/hgiVk/vulkan.h"

HgiVkRenderPass::HgiVkRenderPass(
    HgiVkDevice* device,
    HgiGraphicsEncoderDesc const& desc)
    : _device(device)
    , _descriptor(desc)
    , _vkRenderPass(nullptr)
    , _vkFramebuffer(nullptr)
    , _lastUsedFrame(0)
{
    // https://github.com/KhronosGroup/Vulkan-Docs/wiki/Synchronization-Examples
    // https://gpuopen.com/vulkan-barriers-explained/
    // http://gpuopen.com/wp-content/uploads/2016/03/VulkanFastPaths.pdf
    // https://www.jeremyong.com/ "vulkan-synchonization-primer-part-ii"

    // XXX We should not set VK_DEPENDENCY_BY_REGION_BIT if the shader is
    // sampling arbitrary pixels from the framebuffer.
    // E.g. screenspace reflection

    // Prevent the render pass cache from deleting this render pass.
    _lastUsedFrame = _device->GetCurrentFrame();

    //
    // Process attachments
    //

    HgiTextureUsage usage = 0;

    HgiAttachmentDescConstPtrVector attachments = GetCombinedAttachments(desc);
    for (HgiAttachmentDesc const* attachDesc : attachments) {
        usage |= _ProcessAttachment(*attachDesc);
    }

    bool isSwapchain = usage & HgiTextureUsageBitsSwapchain;

    //
    // SubPasses
    //
    // Each RenderPass can have a number of sub-passes where each subpass
    // uses the same attachment, but in potentially different ways. One subpass
    // may write to an attachment where another subpass reads from it.
    // An example of a using multiple subpasses is be doing a horizontal
    // blur followed by a vertical blur.
    //

    VkSubpassDescription subpassDescription = {};
    subpassDescription.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpassDescription.pColorAttachments = _vkReferences.data();
    subpassDescription.colorAttachmentCount =
        (uint32_t)desc.colorAttachments.size();

    if (desc.depthAttachment.texture) {
        subpassDescription.pDepthStencilAttachment =
            &_vkReferences[desc.colorAttachments.size()];
    }

    //
    // SubPass dependencies
    //
    // Use subpass dependencies to transition image layouts and act as barrier
    // to ensure the read and write operations happen when it is allowed.
    //
    VkSubpassDependency dependencies[2];

    // Start of subpass -- ensure shader reading is completed before FB write.
    dependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
    dependencies[0].dstSubpass = 0;
    dependencies[0].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

    dependencies[0].srcStageMask = isSwapchain ?
        VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT :
        VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;

    dependencies[0].dstStageMask= VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

    dependencies[0].srcAccessMask = isSwapchain ?
        VK_ACCESS_MEMORY_READ_BIT :
        HgiVkRenderPass::GetDefaultDstAccessMask();

    dependencies[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT |
                                    VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

    // End of subpass -- ensure FB write is finished before shader reads.
    dependencies[1].srcSubpass = 0;
    dependencies[1].dstSubpass = VK_SUBPASS_EXTERNAL;
    dependencies[1].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

    dependencies[1].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependencies[1].dstStageMask = isSwapchain ?
        VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT :
        VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;

    dependencies[1].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT |
                                    VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

    dependencies[1].dstAccessMask = isSwapchain ?
        VK_ACCESS_MEMORY_READ_BIT :
        HgiVkRenderPass::GetDefaultDstAccessMask();

    //
    // Create the renderpass
    //
    VkRenderPassCreateInfo renderPassInfo = {};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPassInfo.attachmentCount = attachments.size();
    renderPassInfo.pAttachments = _vkDescriptions.data();
    renderPassInfo.subpassCount = 1;
    renderPassInfo.pSubpasses = &subpassDescription;
    renderPassInfo.dependencyCount = 2;
    renderPassInfo.pDependencies = &dependencies[0];

    TF_VERIFY(
        vkCreateRenderPass(
            _device->GetVulkanDevice(),
            &renderPassInfo,
            HgiVkAllocator(),
            &_vkRenderPass) == VK_SUCCESS
    );

    VkFramebufferCreateInfo fbufCreateInfo =
        {VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO};
    fbufCreateInfo.renderPass = _vkRenderPass;
    fbufCreateInfo.attachmentCount = attachments.size();
    fbufCreateInfo.pAttachments = _vkImageViews.data();
    fbufCreateInfo.width = desc.width;
    fbufCreateInfo.height = desc.height;
    fbufCreateInfo.layers = 1;

    TF_VERIFY(
        vkCreateFramebuffer(
            _device->GetVulkanDevice(),
            &fbufCreateInfo,
            HgiVkAllocator(),
            &_vkFramebuffer) == VK_SUCCESS
    );
}

HgiVkRenderPass::~HgiVkRenderPass()
{
    vkDestroyFramebuffer(
        _device->GetVulkanDevice(),
        _vkFramebuffer,
        HgiVkAllocator());

    vkDestroyRenderPass(
        _device->GetVulkanDevice(),
        _vkRenderPass,
        HgiVkAllocator());
}

bool
HgiVkRenderPass::AcquireRenderPass()
{
    bool inUse = _acquired.test_and_set(std::memory_order_acquire);
    return !inUse;
}

void
HgiVkRenderPass::ReleaseRenderPass()
{
    _acquired.clear(std::memory_order_release);
}

void
HgiVkRenderPass::BeginRenderPass(
    HgiVkCommandBuffer* cb,
    bool usesSecondaryCommandBuffers)
{
    // Prevent the render pass cache from deleting this render pass
    _lastUsedFrame = _device->GetCurrentFrame();

    // Begin render pass in primary command buffer
    VkRenderPassBeginInfo renderPassBeginInfo =
        {VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
    renderPassBeginInfo.renderPass = _vkRenderPass;
    renderPassBeginInfo.framebuffer = _vkFramebuffer;
    renderPassBeginInfo.renderArea.extent.width = _descriptor.width;
    renderPassBeginInfo.renderArea.extent.height = _descriptor.height;
    renderPassBeginInfo.clearValueCount = _vkClearValues.size();
    renderPassBeginInfo.pClearValues = _vkClearValues.data();

    VkSubpassContents contents = usesSecondaryCommandBuffers ?
        VK_SUBPASS_CONTENTS_SECONDARY_COMMAND_BUFFERS :
        VK_SUBPASS_CONTENTS_INLINE;

    vkCmdBeginRenderPass(
        cb->GetCommandBufferForRecoding(),
        &renderPassBeginInfo,
        contents);
}

void
HgiVkRenderPass::EndRenderPass(HgiVkCommandBuffer* cb)
{
    vkCmdEndRenderPass(cb->GetCommandBufferForRecoding());
}

VkRenderPass
HgiVkRenderPass::GetVulkanRenderPass() const
{
    return _vkRenderPass;
}

VkFramebuffer
HgiVkRenderPass::GetVulkanFramebuffer() const
{
    return _vkFramebuffer;
}

HgiGraphicsEncoderDesc const&
HgiVkRenderPass::GetDescriptor() const
{
    return _descriptor;
}

std::vector<VkImageView> const&
HgiVkRenderPass::GetImageViews() const
{
    return _vkImageViews;
}

HgiAttachmentDescConstPtrVector
HgiVkRenderPass::GetCombinedAttachments(HgiGraphicsEncoderDesc const& desc)
{
    HgiAttachmentDescConstPtrVector vec;
    vec.reserve(desc.colorAttachments.size()+1);

    for (uint8_t i=0; i<desc.colorAttachments.size(); i++) {
        vec.push_back(&desc.colorAttachments[i]);
    }

    if (desc.depthAttachment.texture) {
        vec.push_back(&desc.depthAttachment);
    }

    return vec;
}

VkAccessFlags
HgiVkRenderPass::GetDefaultDstAccessMask()
{
    // We are currently not tracking the 'dstAccessMask' state a texture is in.
    // So when a render pass or other command transitions the image to a
    // different dstAccessMask for a command to operate on the texture, we want
    // that cmd to transition the image back to this default mask.
    // For example HgiVkBlitEncoder::CopyTextureGpuToCpu transitions an texture
    // TRANSFER_READ_BIT to copy the texture into a buffer.
    // It will transfer it back to SHADER_READ so the next render pass that
    // uses the texture knows that its current mask is SHADER_READ

    // XXX Performance warning:
    // Currently we always transition back to SHADER_READ at the end of a
    // renderpass (see HgiVkRenderPass constructor) as that is the most likely
    // next usage of a color target. A render-graph system could perhaps
    // give us more fine-tuned transition and shader stage information and that
    // will likely be better for performance.

    return VK_ACCESS_SHADER_READ_BIT;
}

uint64_t
HgiVkRenderPass::GetLastUsedFrame() const
{
    return _lastUsedFrame;
}

HgiTextureUsage
HgiVkRenderPass::_ProcessAttachment(HgiAttachmentDesc const& attachment)
{
    HgiVkTexture const* tex =
        static_cast<HgiVkTexture const*>(attachment.texture);
    if (!TF_VERIFY(tex)) return HgiTextureUsageBitsUndefined;

    HgiTextureDesc const& texDesc = tex->GetDescriptor();

    bool isDepthBuffer = texDesc.usage & HgiTextureUsageBitsDepthTarget;
    bool isSwapchain = texDesc.usage & HgiTextureUsageBitsSwapchain;

    VkAttachmentReference ref;
    ref.attachment = _vkImageViews.size();

    // While HdFormat/HgiFormat do not support BGRA channel ordering it may
    // be used for the native window swapchain on some platforms.
    VkFormat format = isDepthBuffer ? VK_FORMAT_D32_SFLOAT_S8_UINT :
                      HgiVkConversions::GetFormat(texDesc.format);
    if (texDesc.usage & HgiTextureUsageBitsBGRA) {
        if (format == VK_FORMAT_R8G8B8A8_UNORM) {
            format = VK_FORMAT_B8G8R8A8_UNORM;
        } else {
            TF_CODING_ERROR("Unknown texture format with BGRA ordering");
        }
    }

    VkAttachmentDescription desc;
    desc.flags = 0;
    desc.format = format;
    desc.samples = HgiVkConversions::GetSampleCount(texDesc.sampleCount);
    desc.loadOp = HgiVkConversions::GetLoadOp(attachment.loadOp);
    desc.storeOp = HgiVkConversions::GetStoreOp(attachment.storeOp);
    desc.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;       // XXX
    desc.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE; // XXX

    // VkAttachmentDescription::initialLayout and finalLayout are specified
    // for the entire pass. And each reference to the same attachment can
    // transition it to another layout with VkAttachmentReference::layout.
    //
    // The attachment and desired layout for the subpass are set in the
    // VkAttachmentReference array, and then the subpass dependency tells
    // the subpass when to change the layout.

    // Layout of image just before RenderPass (here we use tex layout, but could
    // also be the finalLayout of a previous render-pass)
    desc.initialLayout = tex->GetImageLayout();

    VkClearValue clearValue;

    if (isDepthBuffer) {
        clearValue.depthStencil.depth = attachment.clearValue[0];
        clearValue.depthStencil.stencil = uint32_t(attachment.clearValue[1]);

        // The layout of the image at the end of the entire pass
        desc.finalLayout = isSwapchain ?
            VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL :
            VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;

        // The desired layout for this image during a subpass
        ref.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    } else {
        clearValue.color.float32[0] = attachment.clearValue[0];
        clearValue.color.float32[1] = attachment.clearValue[1];
        clearValue.color.float32[2] = attachment.clearValue[2];
        clearValue.color.float32[3] = attachment.clearValue[3];

        // The layout of the image at the end of the entire pass
        desc.finalLayout = isSwapchain ?
            VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL :
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

        // The desired layout for this image during a subpass
        ref.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    }

    _vkImageViews.push_back(tex->GetImageView());
    _vkClearValues.emplace_back(std::move(clearValue));
    _vkDescriptions.emplace_back(std::move(desc));
    _vkReferences.emplace_back(std::move(ref));

    return texDesc.usage;
}
