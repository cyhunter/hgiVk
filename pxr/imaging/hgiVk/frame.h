#ifndef PXR_IMAGING_HGIVK_FRAME_H
#define PXR_IMAGING_HGIVK_FRAME_H

#include <atomic>
#include <vector>

#include "pxr/pxr.h"
#include "pxr/imaging/hgiVk/api.h"
#include "pxr/imaging/hgiVk/commandBufferManager.h"
#include "pxr/imaging/hgiVk/garbageCollector.h"
#include "pxr/imaging/hgiVk/vulkan.h"


PXR_NAMESPACE_OPEN_SCOPE

class HgiVkDevice;


/// \class HgiVkRenderFrame
///
/// A frame is used to let the CPU record a new frame while the GPU is
/// processing an older frame. This safeguards the data the GPU is consuming,
/// by introducing some latency between cpu writes and gpu reads.
///
/// Deletion of objects must also take care not to delete objects still being
/// consumed by the GPU. The frame has a 'garbage collector' that handles this.
///
class HgiVkRenderFrame final {
public:
    HgiVkRenderFrame(HgiVkDevice* device);
    ~HgiVkRenderFrame();

    /// Should be called exactly once at the start of rendering an app frame.
    HGIVK_API
    void BeginFrame(uint64_t frame);

    /// Should be called exactly once at the end of rendering an app frame.
    HGIVK_API
    void EndFrame();

    /// Returns the garbage collector of the frame.
    HGIVK_API
    HgiVkGarbageCollector* GetGarbageCollector();

    /// Returns the command buffer manager of the frame.
    HGIVK_API
    HgiVkCommandBufferManager* GetCommandBufferManager();

private:
    HgiVkRenderFrame() = delete;
    HgiVkRenderFrame & operator=(const HgiVkRenderFrame&) = delete;
    HgiVkRenderFrame(const HgiVkRenderFrame&) = delete;

private:
    HgiVkDevice* _device;

    // Thread-safe managing of one frame's command buffers.
    HgiVkCommandBufferManager _commandBufferManager;

    // This fence is used to make sure the cpu does not re-use the command
    // buffers until the gpu has finished consuming them.
    VkFence _vkFence;

    // Expired objects (deferred deleted when no longer used by gpu)
    HgiVkGarbageCollector _garbageCollector;
};

typedef std::vector<HgiVkRenderFrame*> HgiVkRenderFrameVector;


PXR_NAMESPACE_CLOSE_SCOPE

#endif
