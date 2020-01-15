#ifndef PXR_IMAGING_HGIVK_COMMAND_BUFFER_H
#define PXR_IMAGING_HGIVK_COMMAND_BUFFER_H

#include <vector>

#include "pxr/pxr.h"
#include "pxr/imaging/hgi/encoderOps.h"
#include "pxr/imaging/hgi/graphicsEncoderDesc.h"
#include "pxr/imaging/hgiVk/api.h"
#include "pxr/imaging/hgiVk/vulkan.h"

#define HGIVK_MAX_TIMESTAMPS 16


PXR_NAMESPACE_OPEN_SCOPE

class HgiVkCommandPool;
class HgiVkDevice;
class HgiVkRenderPass;


/// \enum HgiVkCommandBufferUsage
///
/// Describes the purpose of the command buffer.
///
/// <ul>
/// <li>HgiVkCommandBufferUsagePrimary:
///   Primary cmd buf.</li>
/// <li>HgiVkCommandBufferUsageSecondaryRenderPass:
///   Secondary cmd buf used during parallel draw commands recording within
///   a render pass.</li>
/// <li>HgiVkCommandBufferUsageSecondaryOther:
///   Secondary cmd buf used during parallel command recording outside of
///   a render pass (ie. non-draw calls).</li>
/// </ul>
///
enum HgiVkCommandBufferUsage {
    HgiVkCommandBufferUsagePrimary = 0,
    HgiVkCommandBufferUsageSecondaryRenderPass,
    HgiVkCommandBufferUsageSecondaryOther,

    HgiVkCommandBufferUsageCount
};


/// \class HgiVkCommandBuffer
///
/// Wrapper for vulkan command buffer
///
class HgiVkCommandBuffer final
{
public:
    HGIVK_API
    HgiVkCommandBuffer(
        HgiVkDevice* device,
        HgiVkCommandPool* commandPool,
        HgiVkCommandBufferUsage usage);

    HGIVK_API
    virtual ~HgiVkCommandBuffer();

    /// When a command buffer is used as a secondary command buffer during
    /// parallel graphics encoding it needs to know the renderpass it will
    /// inherit from (the render pass that is begin/ended in the primary
    /// command buffer).
    HGIVK_API
    void SetRenderPass(HgiVkRenderPass* rp);

    /// End recording for command buffer.
    HGIVK_API
    void EndRecording();

    /// Returns true if the command buffer has been used this frame.
    HGIVK_API
    bool IsRecording() const;

    /// Ensures the command buffer is ready to record commands and returns the
    /// vulkan command buffer.
    HGIVK_API
    VkCommandBuffer GetCommandBufferForRecoding();

    /// Returns the vulkan command buffer. Makes no attempt to ensure the
    /// command buffer is ready to record (see AcquireVulkanCommandBuffer);
    HGIVK_API
    VkCommandBuffer GetVulkanCommandBuffer() const;

    /// Set debug name.
    HGIVK_API
    void SetDebugName(const char* name);

    /// Push a time stamp onto stack. This records the start time (TOP_OF_PIPE).
    HGIVK_API
    void PushTimeQuery(const char* name);

    /// Pop last timestamp of stack. This records the end time (BOTTOM_OF_PIPE).
    HGIVK_API
    void PopTimeQuery();

    /// Returns the list of recorded time queries.
    /// This must be called after EndRecording & before recording is re-started.
    HGIVK_API
    HgiTimeQueryVector const& GetTimeQueries();

    /// Reset time queries. This must be called before any render pass begins.
    /// It is called from the command buffer manager at BeginFrame.
    /// Reset happens in the provided cmd buf, not the internal cmd buf.
    HGIVK_API
    void ResetTimeQueries(HgiVkCommandBuffer* cb);

private:
    HgiVkCommandBuffer() = delete;
    HgiVkCommandBuffer & operator= (const HgiVkCommandBuffer&) = delete;
    HgiVkCommandBuffer(const HgiVkCommandBuffer&) = delete;

    // Ensures a command buffer is ready to record commands.
    void _BeginRecording();

private:
    HgiVkDevice* _device;
    HgiVkCommandPool* _commandPool;
    HgiVkCommandBufferUsage _usage;

    bool _isRecording;

    VkCommandBuffer _vkCommandBuffer;
    VkCommandBufferInheritanceInfo _vkInheritanceInfo;

    VkQueryPool _vkTimeStampQueryPool;
    HgiTimeQueryVector _timeQueries;
    bool _timeQueriesReset;
};

typedef std::vector<HgiVkCommandBuffer*> HgiVkCommandBufferVector;


PXR_NAMESPACE_CLOSE_SCOPE

#endif
