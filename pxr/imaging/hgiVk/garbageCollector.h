#ifndef PXR_IMAGING_HGIVK_GARBAGE_COLLECTOR_H
#define PXR_IMAGING_HGIVK_GARBAGE_COLLECTOR_H

#include <atomic>
#include <vector>

#include "pxr/pxr.h"

#include "pxr/imaging/hgiVk/api.h"
#include "pxr/imaging/hgiVk/object.h"

PXR_NAMESPACE_OPEN_SCOPE


/// \class HgiVkGarbageCollector
///
/// Destroys vulkan objects.
///
class HgiVkGarbageCollector final
{
public:
    HGIVK_API
    HgiVkGarbageCollector();

    HGIVK_API
    virtual ~HgiVkGarbageCollector();

    /// Schedule deletion of a vulkan object.
    /// Deletion of all objects must happen via this method since we can have
    /// multiple frames of cmd buffers in-flight and deletion of the object must
    /// wait until no cmd buffers are using the object anymore.
    /// For this reason, vulkan object deletion (and GPU memory release) may be
    /// delayed by several frames.
    HGIVK_API
    void ScheduleObjectDestruction(HgiVkObject const& object);

    /// Destroys objects that were scheduled for destruction.
    /// This should be called once on the oldest render frame.
    HGIVK_API
    void DestroyGarbage(uint64_t frame);

private:
    HgiVkGarbageCollector & operator=(const HgiVkGarbageCollector&) = delete;
    HgiVkGarbageCollector(const HgiVkGarbageCollector&) = delete;

    typedef std::vector<HgiVkObject> VkObjectVector;
    typedef std::vector<VkObjectVector> VkObjectThreadLocalVector;

    // Per-thread vectors of objects to be deleted in the future.
    std::atomic<uint16_t> _numUsedExpired;
    VkObjectThreadLocalVector _expiredVulkanObjects;

    // Frame information
    uint64_t _frame;
    bool _frameStarted;
};

PXR_NAMESPACE_CLOSE_SCOPE

#endif
