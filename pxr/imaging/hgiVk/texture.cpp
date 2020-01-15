#include "pxr/imaging/hgiVk/device.h"
#include "pxr/imaging/hgiVk/diagnostic.h"
#include "pxr/imaging/hgiVk/commandBuffer.h"
#include "pxr/imaging/hgiVk/conversions.h"
#include "pxr/imaging/hgiVk/renderPass.h"
#include "pxr/imaging/hgiVk/texture.h"

PXR_NAMESPACE_OPEN_SCOPE

static bool
_CheckFormatSupport(
    VkPhysicalDevice pDevice,
    VkFormat format,
    VkFormatFeatureFlags flags )
{
    VkFormatProperties props;
    vkGetPhysicalDeviceFormatProperties(pDevice, format, &props);
    return (props.optimalTilingFeatures & flags) == flags;
}

HgiVkTexture::HgiVkTexture(
    HgiVkDevice* device,
    HgiVkCommandBuffer* cb,
    HgiTextureDesc const & desc)
    : HgiTexture(desc)
    , _device(device)
    , _descriptor(desc)
    , _vkImage(nullptr)
    , _vmaImageAllocation(nullptr)
{
    TF_VERIFY(device && cb);

    VkPhysicalDeviceProperties vkDeviceProps =
        device->GetVulkanPhysicalDeviceProperties();

    VkPhysicalDeviceFeatures vkDeviceFeatures =
        device->GetVulkanPhysicalDeviceFeatures();

    GfVec3i const& dimensions = desc.dimensions;
    bool isDepthBuffer = desc.usage & HgiTextureUsageBitsDepthTarget;
    bool supportAnisotropy = vkDeviceFeatures.samplerAnisotropy;

    //
    // Gather image create info
    //

    VkImageCreateInfo imageCreateInfo = {VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};

    imageCreateInfo.imageType = dimensions[2] > 1 ? VK_IMAGE_TYPE_3D :
                                dimensions[1] > 1 ? VK_IMAGE_TYPE_2D :
                                VK_IMAGE_TYPE_1D;

    imageCreateInfo.format = isDepthBuffer ? VK_FORMAT_D32_SFLOAT_S8_UINT :
                              HgiVkConversions::GetFormat(desc.format);

    imageCreateInfo.mipLevels = desc.mipLevels;
    imageCreateInfo.arrayLayers = desc.layerCount;
    imageCreateInfo.samples= HgiVkConversions::GetSampleCount(desc.sampleCount);
    imageCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    imageCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageCreateInfo.extent = {
        (uint32_t) dimensions[0],
        (uint32_t) dimensions[1],
        (uint32_t) dimensions[2]};

    imageCreateInfo.usage = HgiVkConversions::GetTextureUsage(desc.usage);
    VkFormatFeatureFlags formatValidationFlags =
        HgiVkConversions::GetFormatFeature(desc.usage);


    if (!_CheckFormatSupport(
            device->GetVulkanPhysicalDevice(),
            imageCreateInfo.format,
            formatValidationFlags)) {
        TF_CODING_ERROR("Image format not supported on device");
    };

    //
    // Create image with memory allocated and bound.
    //

    // Equivalent to: vkCreateImage, vkAllocateMemory, vkBindImageMemory
    VmaAllocationCreateInfo allocInfo = {};
    allocInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
    TF_VERIFY(
        vmaCreateImage(
            device->GetVulkanMemoryAllocator(),
            &imageCreateInfo,
            &allocInfo,
            &_vkImage,
            &_vmaImageAllocation,
            nullptr) == VK_SUCCESS
    );

    // Debug label
    if (!_descriptor.debugName.empty()) {
        std::string debugLabel = "Image " + _descriptor.debugName;
        HgiVkSetDebugName(
            _device,
            (uint64_t)_vkImage,
            VK_DEBUG_REPORT_OBJECT_TYPE_IMAGE_EXT,
            debugLabel.c_str());
    }

    //
    // Create a texture sampler
    //

    // In Vulkan textures are accessed by samplers.
    // This separates all the sampling information from the texture data.
    // This means you could have multiple sampler objects for the same texture
    // with different settings
    // Note: Similar to the samplers available with OpenGL 3.3+
    // XXX Hgi currently provides no sampler information so we guess.

    VkSamplerCreateInfo sampler = {VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
    sampler.magFilter = VK_FILTER_LINEAR;
    sampler.minFilter = VK_FILTER_LINEAR;
    sampler.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    sampler.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    sampler.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    sampler.compareOp = VK_COMPARE_OP_NEVER;
    sampler.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
    sampler.mipLodBias = 0.0f;
    sampler.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    sampler.minLod = 0.0f;
    sampler.maxLod = (float) desc.mipLevels;
    sampler.maxAnisotropy = supportAnisotropy ?
                            vkDeviceProps.limits.maxSamplerAnisotropy : 1.0f;
    sampler.anisotropyEnable = supportAnisotropy ? VK_TRUE : VK_FALSE;

    TF_VERIFY(
        vkCreateSampler(
            device->GetVulkanDevice(),
            &sampler,
            HgiVkAllocator(),
            &_vkDescriptor.sampler) == VK_SUCCESS
    );

    // Debug label
    if (!_descriptor.debugName.empty()) {
        std::string debugLabel = "Sampler " + _descriptor.debugName;
        HgiVkSetDebugName(
            _device,
            (uint64_t)_vkDescriptor.sampler,
            VK_DEBUG_REPORT_OBJECT_TYPE_SAMPLER_EXT,
            debugLabel.c_str());
    }

    //
    // Create image view
    //

    // Textures are not directly accessed by the shaders and
    // are abstracted by image views containing additional
    // information and sub resource ranges
    VkImageViewCreateInfo view = {VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};

    view.viewType = dimensions[2] > 1 ? VK_IMAGE_VIEW_TYPE_3D :
                    dimensions[1] > 1 ? VK_IMAGE_VIEW_TYPE_2D :
                    VK_IMAGE_VIEW_TYPE_1D;

    view.format = imageCreateInfo.format;
    view.components = { VK_COMPONENT_SWIZZLE_R,
                        VK_COMPONENT_SWIZZLE_G,
                        VK_COMPONENT_SWIZZLE_B,
                        VK_COMPONENT_SWIZZLE_A };

    // The subresource range describes the set of mip levels (and array layers)
    // that can be accessed through this image view.
    // It's possible to create multiple image views for a single image referring
    // to different (and/or overlapping) ranges of the image
    view.subresourceRange.aspectMask = isDepthBuffer ?
        VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT :
        VK_IMAGE_ASPECT_COLOR_BIT;

    view.subresourceRange.baseMipLevel = 0;
    view.subresourceRange.baseArrayLayer = 0;
    view.subresourceRange.layerCount = desc.layerCount;

    if (imageCreateInfo.tiling != VK_IMAGE_TILING_OPTIMAL && desc.mipLevels>1) {
        TF_WARN("linear tiled images usually do not support mips");
    }

    view.subresourceRange.levelCount = desc.mipLevels;

    view.image = _vkImage;

    TF_VERIFY(
        vkCreateImageView(
            device->GetVulkanDevice(),
            &view,
            HgiVkAllocator(),
            &_vkDescriptor.imageView) == VK_SUCCESS
    );

    // Debug label
    if (!_descriptor.debugName.empty()) {
        std::string debugLabel = "Image View " + _descriptor.debugName;
        HgiVkSetDebugName(
            _device,
            (uint64_t)_vkDescriptor.imageView,
            VK_DEBUG_REPORT_OBJECT_TYPE_IMAGE_VIEW_EXT,
            debugLabel.c_str());
    }

    //
    // Transition image
    //

    _vkDescriptor.imageLayout = VK_IMAGE_LAYOUT_UNDEFINED;

// todo Storage images should use VK_IMAGE_LAYOUT_GENERAL

    VkImageLayout newLayout = isDepthBuffer ?
        VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL :
        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    if (imageCreateInfo.usage & VK_IMAGE_USAGE_SAMPLED_BIT) {
        newLayout = isDepthBuffer ?
            VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL :
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    }

    // XXX Optimization potential: Most textures are not read in the
    // vertex stage, so we could use VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT.
    // However keep in mind the depth buffer can also be used in
    // VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT.
    // For now we are conservative and use VERTEX_SHADER.

    // Transition image to SHADER_READ as our default state
    TransitionImageBarrier(
        cb,
        this,
        newLayout, // transition tex to this layout
        HgiVkRenderPass::GetDefaultDstAccessMask(), // shader read access
        VK_PIPELINE_STAGE_TRANSFER_BIT,             // producer stage
        VK_PIPELINE_STAGE_VERTEX_SHADER_BIT);       // consumer stage

    // Don't hold onto pixel data ptr locally. HgiTextureDesc states that:
    // "The application may alter or free this memory as soon as the constructor
    //  of the HgiTexture has returned."
    _descriptor.pixelData = nullptr;
}

HgiVkTexture::HgiVkTexture(
    HgiVkDevice* device,
    HgiTextureDesc const & desc,
    VkDescriptorImageInfo const& vkDesc)
    : HgiTexture(desc)
    , _device(device)
    , _descriptor(desc)
    , _vkDescriptor(vkDesc)
    , _vkImage(nullptr)
    , _vmaImageAllocation(nullptr)
{
    // This constructor directly initialized the vulkan resources (_vkImage).
    // This is useful for images that have their lifetime externally managed.
    // Primarily used for images that are part of the swapchain.
    TF_VERIFY(_descriptor.usage == HgiTextureUsageBitsUndefined ||
              _descriptor.usage & HgiTextureUsageBitsSwapchain);
}

HgiVkTexture::~HgiVkTexture()
{
    // Swapchain image lifetimes are managed internally by the swapchain.
    // We should not attempt to destroy their vulkan resources here.
    if (_descriptor.usage == HgiTextureUsageBitsUndefined ||
        _descriptor.usage & HgiTextureUsageBitsSwapchain) {
        return;
    }

    vkDestroyImageView(
        _device->GetVulkanDevice(),
        _vkDescriptor.imageView,
        HgiVkAllocator());

    vkDestroySampler(
        _device->GetVulkanDevice(),
        _vkDescriptor.sampler,
        HgiVkAllocator());

    vmaDestroyImage(
        _device->GetVulkanMemoryAllocator(),
        _vkImage,
        _vmaImageAllocation);
}

VkImage
HgiVkTexture::GetImage() const
{
    return _vkImage;
}

VkImageView
HgiVkTexture::GetImageView() const
{
    return _vkDescriptor.imageView;
}

VkImageLayout
HgiVkTexture::GetImageLayout() const
{
    return _vkDescriptor.imageLayout;
}

VkSampler
HgiVkTexture::GetSampler() const
{
    return _vkDescriptor.sampler;
}

HgiTextureDesc const&
HgiVkTexture::GetDescriptor() const
{
    return _descriptor;
}

void
HgiVkTexture::CopyTextureFrom(
    HgiVkCommandBuffer* cb,
    HgiVkBuffer const& src)
{
    // Setup buffer copy regions for each mip level
    std::vector<VkBufferImageCopy> bufferCopyRegions;

    uint32_t bpp = HgiVkConversions::GetBytesPerPixel(_descriptor.format);

    float width = (float) _descriptor.dimensions[0];
    float height = (float) _descriptor.dimensions[1];
    float depth = (float) _descriptor.dimensions[2];

    // See dimension reduction rule in ARB_texture_non_power_of_two.
    // Default numMips is: 1 + floor(log2(max(w, h, d)));

    uint32_t offset = 0;
    for (uint32_t i = 0; i < _descriptor.mipLevels; i++) {
        float div = powf(2, i);
        float mipWidth = std::max(1.0f, std::floor(width / div));
        float mipHeight = std::max(1.0f, std::floor(height / div));
        float mipDepth = std::max(1.0f, std::floor(depth / div));
        VkBufferImageCopy bufferCopyRegion = {};
        bufferCopyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        bufferCopyRegion.imageSubresource.mipLevel = i;
        bufferCopyRegion.imageSubresource.baseArrayLayer = 0;
        bufferCopyRegion.imageSubresource.layerCount = _descriptor.layerCount;
        bufferCopyRegion.imageExtent.width = (uint32_t) mipWidth;
        bufferCopyRegion.imageExtent.height = (uint32_t) mipHeight;
        bufferCopyRegion.imageExtent.depth = (uint32_t) mipDepth;
        bufferCopyRegion.bufferOffset = offset;

        bufferCopyRegions.push_back(bufferCopyRegion);

        // Determine byte-offset in total pixel buffer for this mip
        offset += (mipWidth * mipHeight * mipDepth * bpp);
    }

    //
    // Image memory barriers for the texture image
    //

    // Transition image so we can copy into it
    TransitionImageBarrier(
        cb,
        this,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, // Transition tex to this layout
        VK_ACCESS_TRANSFER_WRITE_BIT,         // Write access to image
        VK_PIPELINE_STAGE_HOST_BIT,           // producer stage
        VK_PIPELINE_STAGE_TRANSFER_BIT);      // consumer stage

    // Copy pixels (all mip levels) from staging buffer to gpu image
    vkCmdCopyBufferToImage(
        cb->GetCommandBufferForRecoding(),
        src.GetBuffer(),
        _vkImage,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        static_cast<uint32_t>(bufferCopyRegions.size()),
        bufferCopyRegions.data());

    // Transition image to SHADER_READ when copy is finished
    TransitionImageBarrier(
        cb,
        this,
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,   // transition tex to this
        HgiVkRenderPass::GetDefaultDstAccessMask(), // Shader read access
        VK_PIPELINE_STAGE_TRANSFER_BIT,             // producer stage
        VK_PIPELINE_STAGE_VERTEX_SHADER_BIT);       // consumer stage
}

void
HgiVkTexture::TransitionImageBarrier(
    HgiVkCommandBuffer* cb,
    HgiVkTexture* tex,
    VkImageLayout newLayout,
    VkAccessFlags accesRequest,
    VkPipelineStageFlags producerStage,
    VkPipelineStageFlags consumerStage)
{
    bool isDepthBuffer = _descriptor.usage & HgiTextureUsageBitsDepthTarget;

    // https://github.com/KhronosGroup/Vulkan-Docs/wiki/Synchronization-Examples
    //
    // https://gpuopen.com/vulkan-barriers-explained/
    // Commands start at TOP_OF_PIPE_BIT and end at BOTTOM_OF_PIPE_BIT
    // Inbetween those are various stages the graphics pipeline flows through.
    // With an image barrier we describe what is the producer stage and what
    // will be the earliest consumer stage. This helps schedule work and avoid
    // wait-bubbles.

    // XXX srcAccessMask=0:
    // Only invalidation barrier, no flush barrier. For read-only resources.
    // Meaning: There are no pending writes. Multiple passes can go back to back
    // which all read the resource.

    VkImageMemoryBarrier barrier[1] = {};
    barrier[0].sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier[0].srcAccessMask = 0;            // what producer does
    barrier[0].dstAccessMask = accesRequest; // what consumer does
    barrier[0].oldLayout = _vkDescriptor.imageLayout;
    barrier[0].newLayout = newLayout;
    barrier[0].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier[0].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier[0].image = _vkImage;
    barrier[0].subresourceRange.aspectMask = isDepthBuffer ?
        VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT :
        VK_IMAGE_ASPECT_COLOR_BIT;
    barrier[0].subresourceRange.levelCount = _descriptor.mipLevels;
    barrier[0].subresourceRange.layerCount = _descriptor.layerCount;

    // Insert a memory dependency at the proper pipeline stages that will
    // execute the image layout transition.

    vkCmdPipelineBarrier(
        cb->GetCommandBufferForRecoding(),
        producerStage,
        consumerStage,
        0, 0, NULL, 0, NULL, 1,
        barrier);

    _vkDescriptor.imageLayout = newLayout;
}

PXR_NAMESPACE_CLOSE_SCOPE
