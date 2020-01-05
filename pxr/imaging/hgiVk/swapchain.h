#ifndef PXR_IMAGING_HGIVK_SWAPCHAIN_H
#define PXR_IMAGING_HGIVK_SWAPCHAIN_H

#include <vector>

#include "pxr/imaging/hgiVk/api.h"
#include "pxr/imaging/hgiVk/surface.h"
#include "pxr/imaging/hgiVk/vulkan.h"

PXR_NAMESPACE_OPEN_SCOPE

class HgiVkCommandBuffer;
class HgiVkDevice;
class HgiVkRenderPass;
class HgiVkTexture;

typedef std::vector<VkImageView> VkImageViewVector;


///
/// \class HgiVkSwapchain
///
/// A swap chain is a set of images used for displaying to a window-surface.
///
class HgiVkSwapchain final {
public:
    HGIVK_API
    HgiVkSwapchain(
        HgiVkDevice* device,
        HgiVkSurfaceHandle surface);

    HGIVK_API
    virtual ~HgiVkSwapchain();

    /// Resizes swapchain if neccesairy and acquire the next image for rendering
    /// into the swapchain. Starts the swapchain render pass so after this call
    /// the swapchain is ready to be rendered into.
    HGIVK_API
    void BeginSwapchain(HgiVkCommandBuffer* cb);

    /// Ends the swapchain render pass. The swapchain can no longer be rendered
    /// into until the next BeginSwapchain call. The swap chain can now be
    /// presented to screen.
    HGIVK_API
    void EndSwapchain(HgiVkCommandBuffer* cb);

    /// Display swapchain on screen / window-surface.
    HGIVK_API
    void PresentSwapchain();

    /// Returns the number of images the swapchain uses.
    HGIVK_API
    uint32_t GetImageCount();

    /// Returns one of the the render passes of the swapchain
    HGIVK_API
    HgiVkRenderPass* GetRenderPass(uint32_t imageIndex);

private:
    HgiVkSwapchain() = delete;
    HgiVkSwapchain & operator=(const HgiVkSwapchain&) = delete;
    HgiVkSwapchain(const HgiVkSwapchain&) = delete;

    // Create a new swapchain (e.g. during resize)
    void _CreateVulkanSwapchain();

    // Called before creating a new swapchain
    void _PreDestroyVulkanSwapchain();

    // Creates a new swapchain and destroys the old.
    void _RecreateSwapChain();

    // Checks if swapchain needs to be re-created (resize or format change)
    void _ResizeSwapchainIfNecessary();

    // Toggle to the next image in the swapchain.
    VkResult _AcquireNextImage();

private:
    HgiVkDevice* _device;
    HgiVkSurface* _surface;

    uint32_t _width;
    uint32_t _height;

    std::vector<HgiVkTexture*> _textures;
    std::vector<HgiVkRenderPass*> _renderPasses;

    uint32_t _imageCount;
    uint32_t _nextImageIndex;

    VkFormat _vkSwapchainFormat;
    std::vector<VkImage> _vkImageWeakPtrs;

    VkSwapchainKHR _vkSwapchain;
    VkSemaphore _vkAcquireSemaphore;
    VkSemaphore _vkReleaseSemaphore;
    VkImageViewVector _vkImageViews;
};

typedef HgiVkSwapchain* HgiVkSwapchainHandle;
typedef std::vector<HgiVkSwapchainHandle> HgiVkSwapchainVector;


PXR_NAMESPACE_CLOSE_SCOPE

#endif
