#ifndef PXR_IMAGING_HGIVK_DEVICE_H
#define PXR_IMAGING_HGIVK_DEVICE_H

#include <mutex>
#include <vector>

#include "pxr/pxr.h"
#include "pxr/imaging/hgiVk/api.h"
#include "pxr/imaging/hgiVk/frame.h"
#include "pxr/imaging/hgiVk/object.h"
#include "pxr/imaging/hgiVk/renderPassPipelineCache.h"
#include "pxr/imaging/hgiVk/shaderCompiler.h"
#include "pxr/imaging/hgiVk/vulkan.h"


PXR_NAMESPACE_OPEN_SCOPE

class HgiVkInstance;
class HgiVkCommands;
class HgiVkCommandBuffer;
class HgiVkCommandBufferManager;
class HgiVkCommandPool;


/// Device configuration settings
enum HgiVkDeviceSettings{
    HgiVkPresentationType = 0,
    HgiVkRingBufferSize = 3
};


/// \class HgiVkDevice
///
/// Vulkan implementation of GPU device.
///
class HgiVkDevice final {
public:
    HGIVK_API
    HgiVkDevice(
        HgiVkInstance* instance,
        HgiVkDeviceSettings deviceType);

    HGIVK_API
    ~HgiVkDevice();

    /// Should be called exactly once at the start of rendering a new app frame.
    HGIVK_API
    void BeginFrame();

    /// Should be called exactly once at the end of rendering an app frame.
    HGIVK_API
    void EndFrame();

    /// Returns the command buffer manager of the current frame.
    /// The command buffer manager is used to acquire a command buffer.
    /// Do not hold onto this ptr. It is valid only for one frame and must be
    /// re-acquired each frame.
    HGIVK_API
    HgiVkCommandBufferManager* GetCommandBufferManager();

    /// Commits provided command buffers to queue.
    /// `fence` is optional and can be nullptr.
    /// Thread safety: This call ensures only one thread can submit at once.
    HGIVK_API
    void SubmitToQueue(
        std::vector<VkSubmitInfo> const& submitInfos,
        VkFence fence);

    /// Returns the vulkan device
    HGIVK_API
    VkDevice GetVulkanDevice() const;

    /// Returns the vulkan physical device
    HGIVK_API
    VkPhysicalDevice GetVulkanPhysicalDevice() const;

    /// Returns the vulkan physical device properties
    HGIVK_API
    VkPhysicalDeviceProperties GetVulkanPhysicalDeviceProperties() const;

    /// Returns the vulkan physical device features
    HGIVK_API
    VkPhysicalDeviceFeatures GetVulkanPhysicalDeviceFeatures() const;

    /// Returns the vulkan memory allocator
    HGIVK_API
    VmaAllocator GetVulkanMemoryAllocator() const;

    /// Returns a render pass for the provided descriptor.
    /// Call ReleaseRenderPass after ending the render pass.
    HGIVK_API
    HgiVkRenderPass* AcquireRenderPass(HgiGraphicsEncoderDesc const& desc);

    /// Releases the usage of the provided render pass.
    /// Another graphics encoder may now re-use this render pass.
    /// This should be called after calling EndRenderPass on the render pass.
    HGIVK_API
    void ReleaseRenderPass(HgiVkRenderPass* rp);

    /// Returns the vulkan device queue.
    HGIVK_API
    VkQueue GetVulkanDeviceQueue() const;

    /// Returns the family index of the vulkan device queue.
    HGIVK_API
    uint32_t GetVulkanDeviceQueueFamilyIndex() const;

    /// Returns the vulkan pipeline cache
    HGIVK_API
    VkPipelineCache GetVulkanPipelineCache() const;

    /// Returns the glsl to SPIRV shader compiler
    HGIVK_API
    HgiVkShaderCompiler* GetShaderCompiler();

    /// Manages deletion of a vulkan object.
    /// Deletion of all objects must happen via this method since we can have
    /// multiple frames of cmd buffers in-flight and deletion of the object must
    /// wait until no cmd buffers are using the object anymore.
    /// For this reason, vulkan object deletion (and GPU memory release) may be
    /// delayed by several frames.
    HGIVK_API
    void DestroyObject(HgiVkObject const& object);

    /// Wait for all queued up commands to have been processed on device.
    /// This should ideally never be used as it creates very big stalls.
    HGIVK_API
    void WaitForIdle();

    /// Returns the (internal) frame counter value.
    HGIVK_API
    uint64_t GetCurrentFrame() const;

    /// Returns device used and unused memmory.
    HGIVK_API
    void GetDeviceMemoryInfo(size_t* used, size_t* unused) const;

    /// Returns true if the device support debug marker extension
    HGIVK_API
    bool GetDeviceSupportDebugMarkers() const;

    /// Returns true if the device support time stamps
    HGIVK_API
    bool GetDeviceSupportTimeStamps() const;

    /// Returns time queries recorded in the previous run of the current frame.
    HgiTimeQueryVector const & GetTimeQueries() const;

private:
    HgiVkDevice() = delete;
    HgiVkDevice & operator=(const HgiVkDevice&) = delete;
    HgiVkDevice(const HgiVkDevice&) = delete;

    // Returns true if the provided extension is supported by the device
    bool _IsSupportedExtension(const char* extensionName) const;

private:
    // Vulkan device objects
    VmaAllocator _vmaAllocator;
    VkPhysicalDevice _vkPhysicalDevice;
    VkPhysicalDeviceProperties _vkDeviceProperties;
    VkPhysicalDeviceFeatures _vkDeviceFeatures;
    VkPhysicalDeviceMemoryProperties _vkMemoryProperties;
    VkDevice _vkDevice;
    uint32_t _vkQueueFamilyIndex;
    VkQueue _vkQueue;
    VkPipelineCache _vkPipelineCache;
    std::vector<VkExtensionProperties> _extensions;
    bool _supportsDebugMarkers;
    bool _supportsTimeStamps;

    // Vulkan queue is externally synchronized
    std::mutex _queuelock;

    // glsl SPIRV shader compiler
    HgiVkShaderCompiler _shaderCompiler;

    // Frame information
    uint64_t _frame;
    bool _frameStarted;

    // Internal cache of render passes that map to client created pipelines.
    HgiVkRenderPassPipelineCache _renderPassPipelineCache;

    // We can have multiple frames in-flight (ring-buffer) where the CPU is
    // recording new command for frame N while the GPU is rendering frame N-2.
    int8_t _ringBufferIndex;
    HgiVkRenderFrameVector _frames;
};


PXR_NAMESPACE_CLOSE_SCOPE

#endif
