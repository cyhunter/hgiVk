#ifndef PXR_IMAGING_HGIVK_COMMAND_BUFFER_MANAGER_H
#define PXR_IMAGING_HGIVK_COMMAND_BUFFER_MANAGER_H

#include <atomic>
#include <memory>
#include <string>

#include "pxr/pxr.h"
#include "pxr/imaging/hgiVk/api.h"
#include "pxr/imaging/hgiVk/commandBuffer.h"
#include "pxr/imaging/hgiVk/commandPool.h"
#include "pxr/imaging/hgiVk/vulkan.h"

PXR_NAMESPACE_OPEN_SCOPE


class HgiVkDevice;
class HgiVkCommandBuffer;


/// \class HgiVkCommandBufferManager
///
/// Manages the creation and thread-safety of command pool and buffers for one
/// render frame. Vulkan command pools & buffers are 'externally synchornized'.
/// Which means we need to ensure only one thread access them at a time.
/// The command buffer manager does this by creating a pool & buffer per thread.
/// Thread local storage is used to assign one pool and buffer to a thread.
///
///
class HgiVkCommandBufferManager final
{
public:
    HGIVK_API
    HgiVkCommandBufferManager(
        HgiVkDevice* device);

    HGIVK_API
    virtual ~HgiVkCommandBufferManager();

    /// Should be called exactly once at the start of rendering an app frame.
    HGIVK_API
    void BeginFrame(uint64_t frame);

    /// Should be called exactly once at the end of rendering an app frame.
    /// The provided fence is submitted to the queue and will be signaled once
    /// the command buffers have been consumed. This would usually be the same
    /// fence that the Frame waits on.
    HGIVK_API
    void EndFrame(VkFence fence);

    /// Returns a (thread_local) resource command buffer.
    /// It is guaranteed the returned command buffer is not currently being
    /// consumed by the GPU.
    /// Thread safety: The returned command buffer is thread_local. It is
    /// guaranteed no other thread will use this command buffer for recording.
    HGIVK_API
    HgiVkCommandBuffer* GetResourceCommandBuffer();

    /// Returns a (thread_local) draw command buffer.
    /// It is guaranteed the returned command buffer is not currently being
    /// consumed by the GPU.
    /// Thread safety: The returned command buffer is thread_local. It is
    /// guaranteed no other thread will use this command buffer for recording.
    HGIVK_API
    HgiVkCommandBuffer* GetDrawCommandBuffer();

    /// Called by each parallel encoder to ensure there is enough secondary
    /// command buffers space available. Returns the 'unique start id' of the
    /// encoder's location in the secondary command buffer vector.
    /// This unique id is needed during GetSecondaryDrawCommandBuffer.
    /// Thread safety: Not thread safe. Must be called before any parallel
    /// rendering begins.
    HGIVK_API
    size_t ReserveSecondaryDrawBuffersForParallelEncoder();

    /// Returns a (thread_local) secondary draw command buffer.
    /// Secondary command buffers are used during parallel graphics encoding to
    /// split draw calls over multiple threads. The 'id' should be the value
    /// returned by ReserveSecondaryDrawBuffersForParallelEncoder().
    /// It is guaranteed the returned command buffer is not currently being
    /// consumed by the GPU.
    /// Thread safety: The returned command buffer is thread_local. It is
    /// guaranteed no other thread will use this command buffer for recording.
    HGIVK_API
    HgiVkCommandBuffer* GetSecondaryDrawCommandBuffer(size_t id);

    /// End recording for the secondary command buffers identified with 'id' and
    /// executes (records) them into the primary command buffer.
    HGIVK_API
    void ExecuteSecondaryCommandBuffers(
        size_t id,
        HgiVkCommandBuffer* primaryCommandBuffer);

    /// Set debug name the vulkan objects held by this manager will have.
    HGIVK_API
    void SetDebugName(std::string const& name);

    /// Returns the time queries of all command buffers of the previous run.
    HGIVK_API
    HgiTimeQueryVector const & GetTimeQueries() const;

private:
    HgiVkCommandBufferManager() = delete;
    HgiVkCommandBufferManager & operator= (
        const HgiVkCommandBufferManager&) = delete;
    HgiVkCommandBufferManager(
        const HgiVkCommandBufferManager&) = delete;

    // Create pools and command buffers for parallel recording.
    void _CreatePoolsAndBuffers();

    // This function ensures that each thread grabs an unique index in the
    // command buffer vectors for the current frame.
    void _UpdateThreadLocalIndex();

private:
    HgiVkDevice* _device;

    uint64_t _frame;

    // This index is reset each frame and allows each thread to grab an
    // unique index into the command buffer and descriptor pool vectors.
    std::atomic<uint16_t> _nextAvailableIndex;

    // These are the primary command buffers.
    // Resource commands and draw commands are split into seperate command
    // buffers so we can submit the resources changes first. We want them to
    // complete before the draw commands begin.
    // The vectors hold a command buffer for each thread.
    HgiVkCommandBufferVector _resourceCommandBuffers;
    HgiVkCommandBufferVector _drawCommandBuffers;

    // Secondary draw command buffers are used to parallize rendering into a
    // render pass. This vector is grown dynamically as needed by parallel
    // command encoders.
    HgiVkCommandBufferVector _secondaryDrawCommandBuffers;

    // One command pool per primary command buffer, per thread.
    HgiVkCommandPoolVector _commandPools;

    // This counter keeps track of how many parallel encoders are used each
    // frame so we can make sure we have enough secondary command buffers.
    uint16_t _parallelEncoderCounter;

    // This semaphore is used to synchronize the submission of resource and draw
    // command buffers.
    VkSemaphore _vkSemaphore;

    // Time queries of previous run.
    HgiTimeQueryVector _timeQueries;

    // Debug label
    std::string _debugName;
};


PXR_NAMESPACE_CLOSE_SCOPE

#endif
