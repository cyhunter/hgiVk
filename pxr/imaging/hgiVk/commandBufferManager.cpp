#include "pxr/base/tf/diagnostic.h"
#include "pxr/imaging/hgiVk/commandBufferManager.h"
#include "pxr/imaging/hgiVk/device.h"
#include "pxr/imaging/hgiVk/diagnostic.h"
#include "pxr/imaging/hgiVk/hgi.h"
#include "pxr/imaging/hgiVk/vulkan.h"


PXR_NAMESPACE_OPEN_SCOPE

thread_local uint16_t _HgiVkcmdBufThreadLocalIndex = 0;
thread_local uint64_t _HgiVkcmdBufThreadLocalFrame = ~0ull;


HgiVkCommandBufferManager::HgiVkCommandBufferManager(HgiVkDevice* device)
    : _device(device)
    , _frame(~0ull)
    , _nextAvailableIndex(0)
    , _parallelEncoderCounter(0)
    , _vkSemaphore(nullptr)
{
    //
    // Create semaphore for gpu-gpu synchronization
    //
    VkSemaphoreCreateInfo semaCreateInfo =
        {VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};

    TF_VERIFY(
        vkCreateSemaphore(
            _device->GetVulkanDevice(),
            &semaCreateInfo,
            HgiVkAllocator(),
            &_vkSemaphore) == VK_SUCCESS
    );
}

HgiVkCommandBufferManager::~HgiVkCommandBufferManager()
{
    // Destroy command buffers
    auto deleteCmdBufsFn = [](HgiVkCommandBufferVector& v) -> void {
        for (HgiVkCommandBuffer* cb : v) {
            delete cb;
        }
    };

    deleteCmdBufsFn(_resourceCommandBuffers);
    deleteCmdBufsFn(_drawCommandBuffers);
    deleteCmdBufsFn(_secondaryDrawCommandBuffers);

    for (HgiVkCommandPool* cp : _commandPools) {
        delete cp;
    }

    vkDestroySemaphore(
        _device->GetVulkanDevice(),
        _vkSemaphore, HgiVkAllocator());
}

void
HgiVkCommandBufferManager::BeginFrame(uint64_t frame)
{
    // Change the frame counter. This will let each thread know that they
    // must re-initializes themselves the next time the thread wants to use
    // the command buffers. They know this by comparing the frame they stored
    // thread-locally with _frame. If the number does not match the threads re-
    // acquire the thread's command buffer.
    _frame = frame;

    // Collect all time queries from previous run before resetting.
    _timeQueries.clear();

    auto mergeQueriesFn = [this](HgiVkCommandBufferVector const& v) -> void {
        for (HgiVkCommandBuffer* cb : v) {
            if (!cb) continue;
            HgiTimeQueryVector const& q = cb->GetTimeQueries();
            _timeQueries.insert(_timeQueries.end(), q.begin(), q.end());
        }
    };

    mergeQueriesFn(_resourceCommandBuffers);
    mergeQueriesFn(_drawCommandBuffers);
    mergeQueriesFn(_secondaryDrawCommandBuffers);


    // Reset all command pools of the frame to re-use the command buffers.
    for (HgiVkCommandPool* cp : _commandPools) {
        cp->ResetCommandPool();
    }

    // Make sure there are enough command buffers and pools. One per thread.
    _CreatePoolsAndBuffers();

    // Reset all time queries for all available command buffers.
    // We do this here instead of in HgiVkCommandBuffer::_BeginRecording,
    // because this reset must happen before any render pass is started.
    // Secondary command buffers are created on-demand so they may not be ready
    // yet. As a consequence they will not be able to record time stamps until
    // a few frames after they have been created.
    HgiVkCommandBuffer* primaryCB = GetResourceCommandBuffer();

    auto resetQueriesFn = [this, primaryCB](HgiVkCommandBufferVector const& v) {
        for (HgiVkCommandBuffer* cb : v) {
            if (!cb) continue;
            cb->ResetTimeQueries(primaryCB);
        }
    };

    resetQueriesFn(_resourceCommandBuffers);
    resetQueriesFn(_drawCommandBuffers);
    resetQueriesFn(_secondaryDrawCommandBuffers);
}

void
HgiVkCommandBufferManager::EndFrame(VkFence fence)
{
    // Build a list of all resource command buffers to submit.
    std::vector<VkCommandBuffer> resourceCmds;
    resourceCmds.reserve(_resourceCommandBuffers.size());

    for (HgiVkCommandBuffer* cb : _resourceCommandBuffers) {
        if (cb->IsRecording()) {
            cb->EndRecording();
            resourceCmds.push_back(cb->GetVulkanCommandBuffer());
        }
    }

    // Build a list of all draw command buffers to submit.
    std::vector<VkCommandBuffer> drawCmds;
    drawCmds.reserve(_drawCommandBuffers.size());

    for (HgiVkCommandBuffer* cb : _drawCommandBuffers) {
        if (cb->IsRecording()) {
            cb->EndRecording();
            drawCmds.push_back(cb->GetVulkanCommandBuffer());
        }
    }

    // We submit resource cmds followed by draw cmds.
    // The draw cmds wait for the resource cmds to signal a semaphore so that
    // all resources are in the correct state before draw cmds use them.
    std::vector<VkSubmitInfo> submitInfos;

    if (!resourceCmds.empty()) {
        VkSubmitInfo resourceInfo = {VK_STRUCTURE_TYPE_SUBMIT_INFO};
        resourceInfo.commandBufferCount = (uint32_t) resourceCmds.size();
        resourceInfo.pCommandBuffers = resourceCmds.data();
        if (!drawCmds.empty()) {
            resourceInfo.signalSemaphoreCount = 1;
            resourceInfo.pSignalSemaphores = &_vkSemaphore;
        }
        submitInfos.emplace_back(std::move(resourceInfo));
    }

    if (!drawCmds.empty()) {
        VkSubmitInfo drawInfo = {VK_STRUCTURE_TYPE_SUBMIT_INFO};
        drawInfo.commandBufferCount = (uint32_t) drawCmds.size();
        drawInfo.pCommandBuffers = drawCmds.data();
        if (!resourceCmds.empty()) {
            drawInfo.waitSemaphoreCount = 1;
            drawInfo.pWaitSemaphores = &_vkSemaphore;
            VkPipelineStageFlags waitMask = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
            drawInfo.pWaitDstStageMask = &waitMask;
        }
        submitInfos.emplace_back(std::move(drawInfo));
    }

    // Commit all recorded resource and draw commands from all threads.
    _device->SubmitToQueue(submitInfos, fence);

    // Next frame's threads must re-acquire a command buffer, so reset index.
    // (Same applies to parallel encoders)
    _nextAvailableIndex.store(0);
    _parallelEncoderCounter = 0;
}

HgiVkCommandBuffer*
HgiVkCommandBufferManager::GetResourceCommandBuffer()
{
    /* MULTI-THREAD CALL*/

    // First ensure this thread has an unique index into the cmd buffer vector.
    _UpdateThreadLocalIndex();

    if (_HgiVkcmdBufThreadLocalIndex >= _resourceCommandBuffers.size()) {
        TF_CODING_ERROR("cmdBuf numThreads > HgiVk::GetThreadCount");
        _HgiVkcmdBufThreadLocalIndex = 0;
    }

    return _resourceCommandBuffers[_HgiVkcmdBufThreadLocalIndex];
}

HgiVkCommandBuffer*
HgiVkCommandBufferManager::GetDrawCommandBuffer()
{
    /* MULTI-THREAD CALL*/

    // First ensure this thread has an unique index into the cmd buffer vector.
    _UpdateThreadLocalIndex();

    if (_HgiVkcmdBufThreadLocalIndex >= _drawCommandBuffers.size()) {
        TF_CODING_ERROR("cmdBuf numThreads > HgiVk::GetThreadCount");
        _HgiVkcmdBufThreadLocalIndex = 0;
    }

    return _drawCommandBuffers[_HgiVkcmdBufThreadLocalIndex];
}

size_t
HgiVkCommandBufferManager::ReserveSecondaryDrawBuffersForParallelEncoder()
{
    size_t index = _parallelEncoderCounter;
    _parallelEncoderCounter++;

    unsigned numThreads = HgiVk::GetThreadCount();
    size_t currentSize = _secondaryDrawCommandBuffers.size();
    size_t requiredSize = _parallelEncoderCounter*numThreads;

    if (requiredSize > currentSize) {
        // Important! We only make room for the extra command buffers, but we do
        // not allocate them until GetSecondaryDrawCommandBuffer().
        // We need to wait until we can be sure the thread has exclusive access
        // to the command pool that will allocate the new command buffer!
        // We can't be fully sure of that here. There may be another thread that
        // is currently doing rendering work (e.g. UI thread).
        // So why make room now? Because the parallel encoder calls this
        // before any threading has started, so we can safely resize the vector.
        // During GetSecondaryDrawCommandBuffer we will be wide and shouldn't
        // change the size of the vector.
        _secondaryDrawCommandBuffers.resize(requiredSize, nullptr);
    }

    return index;
}

HgiVkCommandBuffer*
HgiVkCommandBufferManager::GetSecondaryDrawCommandBuffer(size_t id)
{
    /* MULTI-THREAD CALL*/

    // First ensure this thread has an unique index into the cmd buffer vector.
    _UpdateThreadLocalIndex();

    if (_HgiVkcmdBufThreadLocalIndex >= _secondaryDrawCommandBuffers.size()) {
        TF_CODING_ERROR("cmdBuf numThreads > HgiVk::GetThreadCount");
        _HgiVkcmdBufThreadLocalIndex = 0;
    }

    unsigned numThreads = HgiVk::GetThreadCount();
    size_t offset = (id * numThreads) + _HgiVkcmdBufThreadLocalIndex;
    TF_VERIFY(offset < _secondaryDrawCommandBuffers.size());

    // If we didn't make the secondary command buffer yet, do so now.
    if (!_secondaryDrawCommandBuffers[offset]) {

        // Important! make sure we always use the same command pool at the
        // same vector index as resource and draw primary command buffers.
        // A threadpool cannot be used by two different threads at the same
        // time. So if a draw command buffer is used by thread-N that same
        // thread-N can only use secondary command buffers that were also
        // created by that same thread pool.
        HgiVkCommandPool* cp = _commandPools[_HgiVkcmdBufThreadLocalIndex];

        HgiVkCommandBuffer* cb = new HgiVkCommandBuffer(
            _device,
            cp,
            HgiVkCommandBufferUsageSecondaryRenderPass);

        std::string debugLabel = "Secondary " + _debugName;
        cb->SetDebugName(debugLabel.c_str());

        _secondaryDrawCommandBuffers[offset] = cb;
    }

    return _secondaryDrawCommandBuffers[offset];
}

void
HgiVkCommandBufferManager::ExecuteSecondaryCommandBuffers(
    size_t id,
    HgiVkCommandBuffer* primaryCommandBuffer)
{
    unsigned numThreads = HgiVk::GetThreadCount();
    size_t begin = id * numThreads;
    size_t end = begin + numThreads;

    size_t vecSize = _secondaryDrawCommandBuffers.size();

    if (!TF_VERIFY(primaryCommandBuffer && end <= vecSize)) {
        return;
    }

    std::vector<VkCommandBuffer> cbs;
    cbs.reserve(end-begin);

    // End recording on all secondary command buffers.
    for (size_t i=begin; i<end; i++) {
        HgiVkCommandBuffer* cb = _secondaryDrawCommandBuffers[i];
        if (cb && cb->IsRecording()) {
            cb->EndRecording();
            cbs.push_back(cb->GetVulkanCommandBuffer());
        }
    }

    // Submit secondary command buffers into primary command buffer.
    if (!cbs.empty()) {
        vkCmdExecuteCommands(
            primaryCommandBuffer->GetVulkanCommandBuffer(),
            (uint32_t) cbs.size(),
            cbs.data());
    }
}

void
HgiVkCommandBufferManager::SetDebugName(std::string const& name)
{
    _debugName = name;

    std::string debugLabel = "Semaphore " + name;
    HgiVkSetDebugName(
        _device,
        (uint64_t)_vkSemaphore,
        VK_DEBUG_REPORT_OBJECT_TYPE_SEMAPHORE_EXT,
        debugLabel.c_str());

    for (HgiVkCommandPool* pool : _commandPools) {
        pool->SetDebugName(name);
    }

    auto setDebugNameFn = [name](HgiVkCommandBufferVector const& v) {
        for (HgiVkCommandBuffer* cb : v) {
            if (!cb) continue;
            cb->SetDebugName(name.c_str());
        }
    };

    setDebugNameFn(_resourceCommandBuffers);
    setDebugNameFn(_drawCommandBuffers);
    setDebugNameFn(_secondaryDrawCommandBuffers);
}

HgiTimeQueryVector const &
HgiVkCommandBufferManager::GetTimeQueries() const
{
    return _timeQueries;
}

void
HgiVkCommandBufferManager::_CreatePoolsAndBuffers()
{
    unsigned numThreads = HgiVk::GetThreadCount();

    // Unlikely, but the max number of threads may have changed between frames.
    // If that happens, we grow the primary command buffers and pools.
    size_t currentSize = _resourceCommandBuffers.size();

    for (size_t i=currentSize; i<numThreads; i++) {
        HgiVkCommandPool* cp = new HgiVkCommandPool(_device);
        _commandPools.push_back(cp);

        _resourceCommandBuffers.push_back(
            new HgiVkCommandBuffer(
                _device,
                cp,
                HgiVkCommandBufferUsagePrimary)
        );

        _drawCommandBuffers.push_back(
            new HgiVkCommandBuffer(
                _device,
                cp,
                HgiVkCommandBufferUsagePrimary)
        );

        // Last loop
        if (i==numThreads-1) {
            // Update debug names on all new pools and buffers
            SetDebugName(_debugName);
        }
    }
}

void
HgiVkCommandBufferManager::_UpdateThreadLocalIndex()
{
    /* MULTI-THREAD CALL*/

    // Hydra will spawn multiple threads when syncing prims.
    // We want each mesh, curve, etc to be able to record vulkan commands into
    // a command buffer for parallel recording.

    // When such a prim asks for a command buffer we determine what buffer to
    // return based on _HgiVkcmdBufThreadLocalIndex.
    // This ensures each thread has an unique command buffer.

    // When the next frame has started, each thread must re-acquire its
    // _HgiVkcmdBufThreadLocalIndex. This protects against threads being
    // created, destroyed or the number of threads changing between frames.

    if (_HgiVkcmdBufThreadLocalFrame != _frame) {
        _HgiVkcmdBufThreadLocalFrame = _frame;
        _HgiVkcmdBufThreadLocalIndex = _nextAvailableIndex.fetch_add(1);
    }
}

PXR_NAMESPACE_CLOSE_SCOPE
