#ifndef PXR_IMAGING_HGIVK_TEXTURE_H
#define PXR_IMAGING_HGIVK_TEXTURE_H


#include "pxr/pxr.h"
#include "pxr/imaging/hgi/texture.h"

#include "pxr/imaging/hgiVk/api.h"
#include "pxr/imaging/hgiVk/buffer.h"
#include "pxr/imaging/hgiVk/vulkan.h"


PXR_NAMESPACE_OPEN_SCOPE

class HgiVkDevice;
class HgiVkCommandBuffer;


/// \class HgiVkTexture
///
/// Represents a GPU texture resource.
///
class HgiVkTexture final : public HgiTexture {
public:
    // Default constructor
    HGIVK_API
    HgiVkTexture(
        HgiVkDevice* device,
        HgiVkCommandBuffer* cb,
        HgiTextureDesc const & desc);

    // Constructor for swapchain images
    HGIVK_API
    HgiVkTexture(
        HgiVkDevice* device,
        HgiTextureDesc const & desc,
        VkDescriptorImageInfo const& vkDesc);

    HGIVK_API
    virtual ~HgiVkTexture();

    /// Returns the image of the texture
    HGIVK_API
    VkImage GetImage() const;

    /// Returns the image view of the texture
    HGIVK_API
    VkImageView GetImageView() const;

    /// Returns the image layout of the texture
    HGIVK_API
    VkImageLayout GetImageLayout() const;

    /// Returns the sampler of the texture.
    HGIVK_API
    VkSampler GetSampler() const;

    /// Returns the descriptor of the texture
    HGIVK_API
    HgiTextureDesc const& GetDescriptor() const;

    /// Records a copy command to copy the data from the provided source buffer
    /// into this (destination) texture. This requires that the source buffer is
    /// setup as a staging buffer (HgiBufferUsageTransferSrc) and that this
    /// (destination) texture has usage: HgiTextureUsageTransferDst.
    HGIVK_API
    void CopyTextureFrom(
        HgiVkCommandBuffer* cb,
        HgiVkBuffer const& src);

    /// Transition image from its current layout to newLayout
    HGIVK_API
    void TransitionImageBarrier(
        HgiVkCommandBuffer* cb,
        HgiVkTexture* tex,
        VkImageLayout newLayout,
        VkAccessFlags accesRequest,
        VkPipelineStageFlags producerStage,
        VkPipelineStageFlags consumerStage);

private:
    HgiVkTexture() = delete;
    HgiVkTexture & operator=(const HgiVkTexture&) = delete;
    HgiVkTexture(const HgiVkTexture&) = delete;

private:
    HgiVkDevice* _device;

    HgiTextureDesc _descriptor;

    VkDescriptorImageInfo _vkDescriptor; // VkSampler,VkImageView,VkImageLayout
    VkImage _vkImage;
    VmaAllocation _vmaImageAllocation;
};


PXR_NAMESPACE_CLOSE_SCOPE

#endif
