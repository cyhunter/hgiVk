#include "pxr/imaging/hgiVk/blitEncoder.h"
#include "pxr/imaging/hgiVk/commandBuffer.h"
#include "pxr/imaging/hgiVk/conversions.h"
#include "pxr/imaging/hgiVk/device.h"
#include "pxr/imaging/hgiVk/diagnostic.h"
#include "pxr/imaging/hgiVk/renderPass.h"
#include "pxr/imaging/hgiVk/texture.h"
#include "pxr/imaging/hgiVk/vulkan.h"

#include "pxr/imaging/hgi/blitEncoderOps.h"

PXR_NAMESPACE_OPEN_SCOPE

HgiVkBlitEncoder::HgiVkBlitEncoder(
    HgiVkDevice* device,
    HgiVkCommandBuffer* cmdBuf)
    : HgiBlitEncoder()
    , _device(device)
    , _commandBuffer(cmdBuf)
    , _isRecording(true)
{

}

HgiVkBlitEncoder::~HgiVkBlitEncoder()
{
    if (_isRecording) {
        EndEncoding();
    }
}

void
HgiVkBlitEncoder::EndEncoding()
{
    _commandBuffer = nullptr;
    _isRecording = false;
}

void
HgiVkBlitEncoder::CopyTextureGpuToCpu(
    HgiTextureGpuToCpuOp const& copyOp)
{
    HgiVkTexture* srcTexture =
        static_cast<HgiVkTexture*>(copyOp.gpuSourceTexture);

    if (!TF_VERIFY(srcTexture && srcTexture->GetImage(),
        "Invalid texture handle")) {
        return;
    }

    if (copyOp.destinationBufferByteSize == 0) {
        TF_WARN("The size of the data to copy was zero (aborted)");
        return;
    }

    HgiTextureDesc const& desc = srcTexture->GetDescriptor();

    uint32_t layerCnt = copyOp.startLayer + copyOp.numLayers;
    if (!TF_VERIFY(desc.layerCount >= layerCnt,
        "Texture has less layers than attempted to be copied")) {
        return;
    }

    // Create a new command pool and command buffer for this command since we
    // need to submit it immediately and wait for it to complete so that the
    // CPU can read the pixels data.
    HgiVkCommandPool cp(_device);
    HgiVkCommandBuffer cb(_device, &cp, HgiVkCommandBufferUsagePrimary);
    VkCommandBuffer vkCmdBuf = cb.GetCommandBufferForRecoding();

    // Create the GPU buffer that will receive a copy of the GPU texels that
    // we can then memcpy to CPU buffer.
    HgiBufferDesc dstDesc;
    dstDesc.usage = HgiBufferUsageTransferDst | HgiBufferUsageGpuToCpu;
    dstDesc.byteSize = copyOp.destinationBufferByteSize;
    dstDesc.data = nullptr;

    HgiVkBuffer dstBuffer(_device, dstDesc);

    // Setup info to copy data form gpu texture to gpu buffer
    HgiTextureDesc const& texDesc = srcTexture->GetDescriptor();

    VkOffset3D imageOffset;
    imageOffset.x = copyOp.sourceTexelOffset[0];
    imageOffset.y = copyOp.sourceTexelOffset[1];
    imageOffset.z = copyOp.sourceTexelOffset[2];

    VkExtent3D imageExtent;
    imageExtent.width = texDesc.dimensions[0];
    imageExtent.height = texDesc.dimensions[1];
    imageExtent.depth = texDesc.dimensions[2];

    VkImageSubresourceLayers imageSub;
    imageSub.aspectMask = HgiVkConversions::GetImageAspectFlag(texDesc.usage);
    imageSub.baseArrayLayer = copyOp.startLayer;
    imageSub.layerCount = copyOp.numLayers;
    imageSub.mipLevel = copyOp.mipLevel;

    // See vulkan docs: Copying Data Between Buffers and Images
    VkBufferImageCopy region;
    region.bufferImageHeight = 0; // Buffer is tightly packed, like image
    region.bufferRowLength = 0;   // Buffer is tightly packed, like image
    region.bufferOffset = (VkDeviceSize) copyOp.destinationByteOffset;
    region.imageExtent = imageExtent;
    region.imageOffset = imageOffset;
    region.imageSubresource = imageSub;

    // Transition image to TRANSFER_READ
    VkImageLayout oldLayout = srcTexture->GetImageLayout();
    srcTexture->TransitionImageBarrier(
        &cb,
        srcTexture,
        VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, // transition tex to this layout
        VK_ACCESS_TRANSFER_READ_BIT,          // type of access
        VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,    // producer stage
        VK_PIPELINE_STAGE_TRANSFER_BIT);      // consumer stage

    // Copy gpu texture to gpu buffer
    vkCmdCopyImageToBuffer(
        vkCmdBuf,
        srcTexture->GetImage(),
        srcTexture->GetImageLayout(),
        dstBuffer.GetBuffer(),
        1,
        &region);

    // Transition image back to what it was.
    srcTexture->TransitionImageBarrier(
        &cb,
        srcTexture,
        oldLayout, // transition tex to this layout
        HgiVkRenderPass::GetDefaultDstAccessMask(), // type of access
        VK_PIPELINE_STAGE_TRANSFER_BIT,             // producer stage
        VK_PIPELINE_STAGE_VERTEX_SHADER_BIT);       // consumer stage

    cb.EndRecording();

    // Create a fence we can block the CPU on until copy is completed
    VkFence vkFence;
    VkFenceCreateInfo fenceInfo = {VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};

    TF_VERIFY(
        vkCreateFence(
            _device->GetVulkanDevice(),
            &fenceInfo,
            HgiVkAllocator(),
            &vkFence) == VK_SUCCESS
    );

    // Submit the command buffer
    std::vector<VkSubmitInfo> submitInfos;
    VkSubmitInfo submitInfo = {VK_STRUCTURE_TYPE_SUBMIT_INFO};
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &vkCmdBuf;
    submitInfos.emplace_back(std::move(submitInfo));
    _device->SubmitToQueue(submitInfos, vkFence);

    // Wait for the copy from GPU to CPU to complete.
    // XXX Performance warning: This call is going to stall CPU.
    TF_VERIFY(
        vkWaitForFences(
            _device->GetVulkanDevice(),
            1,
            &vkFence,
            VK_TRUE,
            100000000000) == VK_SUCCESS
    );

    vkDestroyFence(_device->GetVulkanDevice(), vkFence, HgiVkAllocator());

    // Copy the data from gpu buffer to cpu destination buffer
    dstBuffer.CopyBufferTo(copyOp.cpuDestinationBuffer);
}

void
HgiVkBlitEncoder::ResolveImage(
    HgiResolveImageOp const& resolveOp)
{
    HgiVkTexture* srcTexture =
        static_cast<HgiVkTexture*>(resolveOp.source);

    HgiVkTexture* dstTexture =
        static_cast<HgiVkTexture*>(resolveOp.destination);

    if (!TF_VERIFY(srcTexture && srcTexture->GetImage() &&
                   dstTexture && dstTexture->GetImage(),
                   "Invalid texture handles")) {
        return;
    }

    // XXX while vkCmdResolveImage appears to succeed for depth_stencil images
    // our current usage is not supported by the vulkan spec.
    // The dst image must be COLOR_ATTACHMENT_BIT and srcImage and dstImage must
    // be created with the same image format. This rules out using this function
    // because DEPTH_STENCIL images cannot contain COLOR_ATTACHMENT_BIT.
    // Instead we need to do this via fullscreen shader render-pass.
    // Once we have a helper in-place to make fullscreen passes easier via
    // HgiVkGraphicsEncoder we can replace all of the below code.
    //
    // Color:
    //    layout (binding = 0, set = 0) uniform sampler2DMS colorTex;
    //    layout(location = 0) out vec4 outputColor;
    //    void main()
    //    {
    //        ivec2 texel = ivec2(gl_FragCoord.xy);
    //        vec4 color0 = texelFetch(colorTex, texel, 0);
    //        vec4 color1 = texelFetch(colorTex, texel, 1);
    //        vec4 color2 = texelFetch(colorTex, texel, 2);
    //        vec4 color3 = texelFetch(colorTex, texel, 3);
    //        outputColor = (color0 + color1 + color2 + color3) * 0.25;
    //    }
    //
    // Depth:
    //    ivec2 texel = ivec2(gl_FragCoord.xy);
    //    outputDepth = texelFetch(depthTex, texel, 0).x;
    bool isDepth =
        dstTexture->GetDescriptor().usage & HgiTextureUsageBitsDepthTarget;
    if (isDepth) {
        TF_WARN("vkCmdResolveImage dst image must be COLOR_TARGET_BIT");
    }

    // XXX Performance warning: We could use pResolveAttachments on the
    // render pass of the color image to resolve it more efficiently.
    // And setup as: STORE_OP_DONT_CARE and TRANSIENT.
    // (Won't apply to depth, because it cannot be in pResolveAttachments)

    // XXX For now we assume this can be recorded as a deferred command.
    // That may not always be what is expected. When the caller wants to
    // RenderBuffer::Resolve() just before RenderBuffer::Map() they expect this
    // to be immediately executed since the CPU is going to read the data.
    // We may need a flag in HgiResolveImageOp to indicate 'immediate'.
    // Then here we need to create a unique command buffer with a fence that we
    // submit immediately and wait for fence to complete.
    // See CopyTextureGpuToCpu.

    HgiVkCommandBufferManager* cbm = _device->GetCommandBufferManager();
    HgiVkCommandBuffer* cb = cbm->GetDrawCommandBuffer();

    // Src must be in TRANSFER_READ/SRC for vkCmdResolveImage
    VkImageLayout oldSrcLayout = srcTexture->GetImageLayout();
    srcTexture->TransitionImageBarrier(
        cb,
        srcTexture,
        VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, // transition tex to this layout
        VK_ACCESS_TRANSFER_READ_BIT,          // type of access
        VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,    // producer stage
        VK_PIPELINE_STAGE_TRANSFER_BIT);      // consumer stage

    // Dst must be in TRANSFER_WRITE/DST for vkCmdResolveImage
    VkImageLayout oldDstLayout = dstTexture->GetImageLayout();
    dstTexture->TransitionImageBarrier(
        cb,
        dstTexture,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, // transition tex to this layout
        VK_ACCESS_TRANSFER_WRITE_BIT,         // type of access
        VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,    // producer stage
        VK_PIPELINE_STAGE_TRANSFER_BIT);      // consumer stage

    // Setup image resolve info
    VkImageSubresourceLayers srcInfo;
    HgiTextureDesc const& srcDesc = srcTexture->GetDescriptor();
    srcInfo.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    srcInfo.baseArrayLayer = 0;
    srcInfo.layerCount = srcDesc.layerCount;
    srcInfo.mipLevel = 0;

    VkImageSubresourceLayers dstInfo;
    HgiTextureDesc const& dstDesc = dstTexture->GetDescriptor();
    dstInfo.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    dstInfo.baseArrayLayer = 0;
    dstInfo.layerCount = dstDesc.layerCount;
    dstInfo.mipLevel = 0;

    VkExtent3D srcExtent;
    srcExtent.width = srcDesc.dimensions[0];
    srcExtent.height = srcDesc.dimensions[1];
    srcExtent.depth = srcDesc.dimensions[2];

    VkImageResolve imageResolve;
    imageResolve.srcSubresource = srcInfo;
    imageResolve.srcOffset = VkOffset3D{0,0,0};
    imageResolve.dstSubresource = dstInfo;
    imageResolve.dstOffset = VkOffset3D{0,0,0};
    imageResolve.extent = srcExtent;

    // Resolve image
    vkCmdResolveImage(
        cb->GetCommandBufferForRecoding(),
        srcTexture->GetImage(),
        srcTexture->GetImageLayout(),
        dstTexture->GetImage(),
        dstTexture->GetImageLayout(),
        1, // region count
        &imageResolve);

    // Transition images back to what they were.
    srcTexture->TransitionImageBarrier(
        cb,
        srcTexture,
        oldSrcLayout, // transition tex to this layout
        HgiVkRenderPass::GetDefaultDstAccessMask(), // type of access
        VK_PIPELINE_STAGE_TRANSFER_BIT,             // producer stage
        VK_PIPELINE_STAGE_VERTEX_SHADER_BIT);       // consumer stage

    // Transition images back to what they were.
    dstTexture->TransitionImageBarrier(
        cb,
        dstTexture,
        oldDstLayout, // transition tex to this layout
        HgiVkRenderPass::GetDefaultDstAccessMask(), // type of access
        VK_PIPELINE_STAGE_TRANSFER_BIT,             // producer stage
        VK_PIPELINE_STAGE_VERTEX_SHADER_BIT);       // consumer stage
}

void
HgiVkBlitEncoder::PushDebugGroup(const char* label)
{
    if (!TF_VERIFY(_isRecording && _commandBuffer)) return;
    HgiVkBeginDebugMarker(_commandBuffer, label);
}

void
HgiVkBlitEncoder::PopDebugGroup()
{
    if (!TF_VERIFY(_isRecording && _commandBuffer)) return;
    HgiVkEndDebugMarker(_commandBuffer);
}

void
HgiVkBlitEncoder::PushTimeQuery(const char* name)
{
    if (!TF_VERIFY(_isRecording && _commandBuffer)) return;
    _commandBuffer->PushTimeQuery(name);
}

void
HgiVkBlitEncoder::PopTimeQuery()
{
    if (!TF_VERIFY(_isRecording && _commandBuffer)) return;
    _commandBuffer->PopTimeQuery();
}

PXR_NAMESPACE_CLOSE_SCOPE
