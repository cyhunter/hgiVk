#include "pxr/base/tf/diagnostic.h"

#include "pxr/imaging/hgiVk/commandBuffer.h"
#include "pxr/imaging/hgiVk/conversions.h"
#include "pxr/imaging/hgiVk/device.h"
#include "pxr/imaging/hgiVk/diagnostic.h"
#include "pxr/imaging/hgiVk/renderPass.h"
#include "pxr/imaging/hgiVk/swapchain.h"
#include "pxr/imaging/hgiVk/texture.h"

PXR_NAMESPACE_OPEN_SCOPE

static VkImageMemoryBarrier
_ImageBarrier(
    VkImage image,
    VkAccessFlags srcAccessMask,
    VkAccessFlags dstAccessMask,
    VkImageLayout oldLayout,
    VkImageLayout newLayout)
{
    VkImageMemoryBarrier result = {VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
    result.srcAccessMask = srcAccessMask;
    result.dstAccessMask = dstAccessMask;
    result.oldLayout = oldLayout;
    result.newLayout = newLayout;
    result.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    result.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    result.image = image;
    // XXX for depth: VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT
    result.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    result.subresourceRange.levelCount = VK_REMAINING_MIP_LEVELS;
    result.subresourceRange.layerCount = VK_REMAINING_ARRAY_LAYERS;
    return result;
}

static VkFormat
_GetSurfaceFormat(
    HgiVkDevice* device,
    HgiVkSurface* surface)
{
    uint32_t formatCount = 0;
    TF_VERIFY(
        vkGetPhysicalDeviceSurfaceFormatsKHR(
            device->GetVulkanPhysicalDevice(),
            surface->GetVulkanSurface(),
            &formatCount,
            nullptr) == VK_SUCCESS
    );
    TF_VERIFY(formatCount > 0);

    std::vector<VkSurfaceFormatKHR> formats(formatCount);

    TF_VERIFY(
        vkGetPhysicalDeviceSurfaceFormatsKHR(
            device->GetVulkanPhysicalDevice(),
            surface->GetVulkanSurface(),
            &formatCount,
            formats.data()) == VK_SUCCESS
    );

    if (formatCount == 1 && formats[0].format == VK_FORMAT_UNDEFINED) {
        return VK_FORMAT_R8G8B8A8_UNORM;
    }

    for (uint32_t i=0; i < formatCount; i++) {
        if (formats[i].format == VK_FORMAT_R8G8B8A8_UNORM ||
            formats[i].format == VK_FORMAT_B8G8R8A8_UNORM) {
            return formats[i].format;
        }
    }

    TF_WARN("Using not supported swapchain format");
    return formats[0].format;
}

static void
_DestroyVulkanSwapchain(
    HgiVkDevice* device,
    VkSwapchainKHR vkSwapchain,
    VkSemaphore vkAcquireSemaphore,
    VkSemaphore vkReleaseSemaphore,
    VkImageViewVector& vkImageViews)
{
    TF_VERIFY(
        vkDeviceWaitIdle(device->GetVulkanDevice()) == VK_SUCCESS
    );

    for (uint32_t i = 0; i < vkImageViews.size(); i++) {
        vkDestroyImageView(
            device->GetVulkanDevice(),
            vkImageViews[i],
            HgiVkAllocator());
    }

    vkDestroySwapchainKHR(
        device->GetVulkanDevice(),
        vkSwapchain,
        HgiVkAllocator());

    vkDestroySemaphore(
        device->GetVulkanDevice(),
        vkReleaseSemaphore,
        HgiVkAllocator());

    vkDestroySemaphore(
        device->GetVulkanDevice(),
        vkAcquireSemaphore,
        HgiVkAllocator());
}

HgiVkSwapchain::HgiVkSwapchain(
    HgiVkDevice* device,
    HgiVkSurfaceHandle surface)
    : _device(device)
    , _surface(surface)
    , _width(0)
    , _height(0)
    , _imageCount(0)
    , _nextImageIndex(0)
    , _vkSwapchainFormat(VK_FORMAT_UNDEFINED)
    , _vkSwapchain(nullptr)
    , _vkAcquireSemaphore(nullptr)
    , _vkReleaseSemaphore(nullptr)
{
    _CreateVulkanSwapchain();
}

HgiVkSwapchain::~HgiVkSwapchain()
{
    _PreDestroyVulkanSwapchain();
    _DestroyVulkanSwapchain(
        _device,
        _vkSwapchain,
        _vkAcquireSemaphore,
        _vkReleaseSemaphore,
        _vkImageViews);
}

void
HgiVkSwapchain::BeginSwapchain(HgiVkCommandBuffer* cb)
{
    HgiVkBeginDebugMarker(cb, "BeginSwapchain");

    // Not all drivers may report 'out-of-data' errors for swapchains so we
    // first do a manual size check.
    _ResizeSwapchainIfNecessary();

    // If we fail to acquire the image because the swapchain is out of data then
    // we must re-create the swapchain AND re-acquire the image.
    VkResult res =  _AcquireNextImage();
    if (res == VK_ERROR_OUT_OF_DATE_KHR || res == VK_SUBOPTIMAL_KHR) {
        _RecreateSwapChain();
        res = _AcquireNextImage();
    }
    TF_VERIFY(res == VK_SUCCESS);

    uint32_t imageIndex = _nextImageIndex;

    // The swapchain image must transition from UNDEFINED to COLOR_ATTACH.
    VkImageMemoryBarrier renderBeginBarrier = _ImageBarrier(
        _vkImageWeakPtrs[imageIndex], 0,
        VK_ACCESS_COLOR_ATTACHMENT_READ_BIT |
        VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
        VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

    vkCmdPipelineBarrier(
        cb->GetCommandBufferForRecoding(),
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        VK_DEPENDENCY_BY_REGION_BIT, 0, 0, 0, 0, 1, &renderBeginBarrier);

    _renderPasses[imageIndex]->BeginRenderPass(cb, /*use secondary*/ false);
}

void
HgiVkSwapchain::EndSwapchain(HgiVkCommandBuffer* cb)
{
    uint32_t imageIndex = _nextImageIndex;

    _renderPasses[imageIndex]->EndRenderPass(cb);

    // The swapchain image must transition from COLOR_ATTACH to PRESENT.
    VkImageMemoryBarrier renderEndBarrier = _ImageBarrier(
        _vkImageWeakPtrs[imageIndex],
        VK_ACCESS_COLOR_ATTACHMENT_READ_BIT |
        VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
        0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);

    vkCmdPipelineBarrier(
        cb->GetCommandBufferForRecoding(),
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_DEPENDENCY_BY_REGION_BIT,
        0, 0, 0, 0, 1, &renderEndBarrier);

    HgiVkEndDebugMarker(cb /*"BeginSwapchain"*/);
}

void
HgiVkSwapchain::PresentSwapchain()
{
    VkPipelineStageFlags submitStageMask =
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

    // See Khronos Vulkan wiki: synchronization-Examples
    VkSubmitInfo submitInfo = {VK_STRUCTURE_TYPE_SUBMIT_INFO};
    submitInfo.waitSemaphoreCount = 1;
    submitInfo.pWaitSemaphores = &_vkAcquireSemaphore;
    submitInfo.pWaitDstStageMask = &submitStageMask;
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = &_vkReleaseSemaphore;

    TF_VERIFY(
        vkQueueSubmit(
            _device->GetVulkanDeviceQueue(),
            1,
            &submitInfo,
            VK_NULL_HANDLE) == VK_SUCCESS
    );

    uint32_t imageIndex = _nextImageIndex;

    VkPresentInfoKHR vkPresentInfo = {VK_STRUCTURE_TYPE_PRESENT_INFO_KHR};
    vkPresentInfo.waitSemaphoreCount = 1;
    vkPresentInfo.pWaitSemaphores = &_vkReleaseSemaphore;
    vkPresentInfo.swapchainCount = 1;
    vkPresentInfo.pSwapchains = &_vkSwapchain;
    vkPresentInfo.pImageIndices = &imageIndex;

    VkResult res = vkQueuePresentKHR(
        _device->GetVulkanDeviceQueue(),
        &vkPresentInfo);

    // If swapchain is out of date here we will catch it next BeginSwapchain.
    TF_VERIFY(res == VK_SUCCESS ||
              res == VK_ERROR_OUT_OF_DATE_KHR ||
              res == VK_SUBOPTIMAL_KHR);
}

uint32_t
HgiVkSwapchain::GetImageCount()
{
    return _imageCount;
}

HgiVkRenderPass*
HgiVkSwapchain::GetRenderPass(uint32_t imageIndex)
{
    if (TF_VERIFY(imageIndex < _renderPasses.size())) {
        return _renderPasses[imageIndex];
    }
    return nullptr;
}

void
HgiVkSwapchain::_CreateVulkanSwapchain()
{
    // Verify old textures and render pass are taken care of.
    // See _RecreateSwapChain() for more info.
    TF_VERIFY(_textures.empty() &&
              _renderPasses.empty() &&
              _vkImageWeakPtrs.empty(),
              "There are undestroyed items left in swapchain");

    //
    // Query Surface info
    //
    VkSurfaceCapabilitiesKHR surfaceCaps;
    TF_VERIFY(
        vkGetPhysicalDeviceSurfaceCapabilitiesKHR(
            _device->GetVulkanPhysicalDevice(),
            _surface->GetVulkanSurface(),
            &surfaceCaps) == VK_SUCCESS
    );

    _width = surfaceCaps.currentExtent.width;
    _height = surfaceCaps.currentExtent.height;
    _vkSwapchainFormat = _GetSurfaceFormat(_device, _surface);

    //
    // Create Swapchain
    //
    VkCompositeAlphaFlagBitsKHR alphaOpaque =
        VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    VkCompositeAlphaFlagBitsKHR preMul =
        VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR;
    VkCompositeAlphaFlagBitsKHR postMul =
        VK_COMPOSITE_ALPHA_POST_MULTIPLIED_BIT_KHR;

    VkCompositeAlphaFlagBitsKHR surfaceComposite =
        (surfaceCaps.supportedCompositeAlpha & alphaOpaque) ?
            VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR :
        (surfaceCaps.supportedCompositeAlpha & preMul) ?
            VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR :
        (surfaceCaps.supportedCompositeAlpha & postMul) ?
            VK_COMPOSITE_ALPHA_POST_MULTIPLIED_BIT_KHR :
        VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR;

    VkSwapchainCreateInfoKHR swapCreateInfo =
        {VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR};
    swapCreateInfo.surface = _surface->GetVulkanSurface();
    swapCreateInfo.minImageCount = std::max(2u, surfaceCaps.minImageCount);
    swapCreateInfo.imageFormat = _vkSwapchainFormat;
    swapCreateInfo.imageColorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
    swapCreateInfo.imageExtent.width = _width;
    swapCreateInfo.imageExtent.height = _height;
    swapCreateInfo.imageArrayLayers = 1;
    swapCreateInfo.imageUsage =
        VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | // for rendering
        VK_IMAGE_USAGE_TRANSFER_DST_BIT;      // for blitting
    swapCreateInfo.queueFamilyIndexCount = 1;
    uint32_t queueFamilyIndex = _device->GetVulkanDeviceQueueFamilyIndex();
    swapCreateInfo.pQueueFamilyIndices = &queueFamilyIndex;
    swapCreateInfo.preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
    swapCreateInfo.compositeAlpha = surfaceComposite;
    swapCreateInfo.presentMode = VK_PRESENT_MODE_FIFO_KHR;
    swapCreateInfo.oldSwapchain = _vkSwapchain;

    TF_VERIFY(
        vkCreateSwapchainKHR(
            _device->GetVulkanDevice(),
            &swapCreateInfo,
            HgiVkAllocator(),
            &_vkSwapchain) == VK_SUCCESS
    );
    TF_VERIFY(_vkSwapchain, "Swapchain invalid");

    // Debug label
    {
        std::string debugLabel = "Swapchain HgiVk";
        HgiVkSetDebugName(
            _device,
            (uint64_t)_vkSwapchain,
            VK_DEBUG_REPORT_OBJECT_TYPE_SWAPCHAIN_KHR_EXT,
            debugLabel.c_str());
    }

    //
    // Get swapchain images
    //
    uint32_t imageCount = 0;
    TF_VERIFY(
        vkGetSwapchainImagesKHR(
            _device->GetVulkanDevice(),
            _vkSwapchain,
            &imageCount,
            0/*images*/) == VK_SUCCESS
    );

    _imageCount = imageCount;
    _vkImageWeakPtrs.resize(imageCount);
    _vkImageViews.resize(imageCount);
    _textures.resize(imageCount);

    TF_VERIFY(
        vkGetSwapchainImagesKHR(
            _device->GetVulkanDevice(),
            _vkSwapchain,
            &imageCount,
            _vkImageWeakPtrs.data()) == VK_SUCCESS
    );

    //
    // Create semaphores for accessing swapchain images
    //
    VkSemaphoreCreateInfo semaCreateInfo =
        {VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
    TF_VERIFY(
        vkCreateSemaphore(
            _device->GetVulkanDevice(),
            &semaCreateInfo,
            HgiVkAllocator(),
            &_vkAcquireSemaphore) == VK_SUCCESS
    );

    // Debug label
    {
        std::string debugLabel = "Semaphore Acquire HgiVk Swapchain";
        HgiVkSetDebugName(
            _device,
            (uint64_t)_vkAcquireSemaphore,
            VK_DEBUG_REPORT_OBJECT_TYPE_SEMAPHORE_EXT,
            debugLabel.c_str());
    }

    TF_VERIFY(
        vkCreateSemaphore(
            _device->GetVulkanDevice(),
            &semaCreateInfo,
            HgiVkAllocator(),
            &_vkReleaseSemaphore) == VK_SUCCESS
    );

    // Debug label
    {
        std::string debugLabel = "Semaphore Release HgiVk Swapchain";
        HgiVkSetDebugName(
            _device,
            (uint64_t)_vkReleaseSemaphore,
            VK_DEBUG_REPORT_OBJECT_TYPE_SEMAPHORE_EXT,
            debugLabel.c_str());
    }

    //
    // Create image views
    //
    for (uint32_t i = 0; i < imageCount; i++) {
        VkImageViewCreateInfo createInfo =
            {VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
        createInfo.image = _vkImageWeakPtrs[i];
        createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        createInfo.format = _vkSwapchainFormat;
        createInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        createInfo.subresourceRange.levelCount = 1;
        createInfo.subresourceRange.layerCount = 1;

        TF_VERIFY(
            vkCreateImageView(
                _device->GetVulkanDevice(),
                &createInfo,
                HgiVkAllocator(),
                &_vkImageViews[i]) == VK_SUCCESS
        );
        TF_VERIFY(_vkImageViews[i], "ImageView creation failed");

        // Debug label
        {
            std::string debugLabel = "ImageView " + std::to_string(i) +
                                     " HgiVk Swapchain";
            HgiVkSetDebugName(
                _device,
                (uint64_t)_vkImageViews[i],
                VK_DEBUG_REPORT_OBJECT_TYPE_IMAGE_VIEW_EXT,
                debugLabel.c_str());
        }
    }

    //
    // Create texture-wrapper for each image of swapchain
    //
    for (uint32_t i = 0; i < imageCount; i++) {
        HgiTextureDesc texDesc;
        texDesc.dimensions = GfVec3i(_width, _height, 1);
        texDesc.format = HgiVkConversions::GetFormat(_vkSwapchainFormat);
        texDesc.pixelData = nullptr;
        texDesc.pixelsByteSize = 0;
        texDesc.sampleCount = HgiSampleCount1; // Vulkan swapchain is never MS
        texDesc.usage = HgiTextureUsageBitsColorTarget |
                        HgiTextureUsageBitsSwapchain;

        // While HdFormat/HgiFormat do not support BGRA channel ordering it may
        // be used for the native window swapchain on some platforms.
        if (_vkSwapchainFormat == VK_FORMAT_B8G8R8A8_UNORM) {
            texDesc.usage |= HgiTextureUsageBitsBGRA;
        }

        VkDescriptorImageInfo texVkDesc;
        texVkDesc.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        texVkDesc.imageView = _vkImageViews[i];
        texVkDesc.sampler = nullptr;

        HgiVkTexture* tex = new HgiVkTexture(_device, texDesc, texVkDesc);
        _textures[i] = tex;
    }

    //
    // Create render passes for each image of swapchain
    //
    for (uint32_t i = 0; i < imageCount; i++) {
        HgiAttachmentDesc attachment;
        attachment.clearValue = GfVec4f(0);
        attachment.loadOp = HgiAttachmentLoadOpClear;
        attachment.storeOp = HgiAttachmentStoreOpStore;
        attachment.texture = _textures[i];

        HgiGraphicsEncoderDesc renderPassDesc;
        renderPassDesc.width = surfaceCaps.currentExtent.width;
        renderPassDesc.height = surfaceCaps.currentExtent.height;
        renderPassDesc.colorAttachments.emplace_back(std::move(attachment));

        HgiVkRenderPass* rp = _device->AcquireRenderPass(renderPassDesc);
        _renderPasses.push_back(rp);
    }
}

void
HgiVkSwapchain::_PreDestroyVulkanSwapchain()
{
    // We do not worry about deleting the old render pass since it is in the
    // render pass cache and will eventually be garbage collected.
    _renderPasses.clear();

    // We must delete the textures we created when we created the swapchain.
    // This will only delete the HgiVkTexture, not the vulkan resources since
    // the vkImages are owned / managed internally by the native window.
    for (HgiVkTexture* tex : _textures) {
        delete tex;
    }
    _textures.clear();

    // The swapchain owns the vkImages. We do not destroy them ourselves.
    _vkImageWeakPtrs.clear();
}

void
HgiVkSwapchain::_RecreateSwapChain()
{
    // We don't fully destroy the old swapchain until after creating the new.
    // This allows for optimizations where the driver may be able to re-use
    // parts of the old swapchain.

    VkSwapchainKHR vkSwapchain = _vkSwapchain;
    VkSemaphore vkAcquireSemaphore = _vkAcquireSemaphore;
    VkSemaphore vkReleaseSemaphore = _vkReleaseSemaphore;
    VkImageViewVector vkImageViews = _vkImageViews;
    _vkImageViews.clear();

    _PreDestroyVulkanSwapchain();
    _CreateVulkanSwapchain();
    _DestroyVulkanSwapchain(
        _device,
        vkSwapchain,
        vkAcquireSemaphore,
        vkReleaseSemaphore,
        vkImageViews);
}

void
HgiVkSwapchain::_ResizeSwapchainIfNecessary()
{
    VkSurfaceCapabilitiesKHR surfaceCaps;
    TF_VERIFY(
        vkGetPhysicalDeviceSurfaceCapabilitiesKHR(
            _device->GetVulkanPhysicalDevice(),
            _surface->GetVulkanSurface(),
            &surfaceCaps) == VK_SUCCESS
    );

    uint32_t newWidth = surfaceCaps.currentExtent.width;
    uint32_t newHeight = surfaceCaps.currentExtent.height;

    if (_width == newWidth && _height == newHeight) {
        return;
    }

    _RecreateSwapChain();
}

VkResult
HgiVkSwapchain::_AcquireNextImage()
{
    return vkAcquireNextImageKHR(
        _device->GetVulkanDevice(),
        _vkSwapchain,
        ~0ull,
        _vkAcquireSemaphore,
        VK_NULL_HANDLE,
        &_nextImageIndex);
}


PXR_NAMESPACE_CLOSE_SCOPE
