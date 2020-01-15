#include <string>

#include "pxr/base/tf/diagnostic.h"
#include "pxr/imaging/hgi/graphicsEncoderDesc.h"
#include "pxr/imaging/hgiVk/commandBuffer.h"
#include "pxr/imaging/hgiVk/commandPool.h"
#include "pxr/imaging/hgiVk/blitEncoder.h"
#include "pxr/imaging/hgiVk/device.h"
#include "pxr/imaging/hgiVk/diagnostic.h"
#include "pxr/imaging/hgiVk/graphicsEncoder.h"
#include "pxr/imaging/hgiVk/hgi.h"
#include "pxr/imaging/hgiVk/renderPass.h"
#include "pxr/imaging/hgiVk/texture.h"
#include "pxr/imaging/hgiVk/vulkan.h"


PXR_NAMESPACE_OPEN_SCOPE


HgiVkCommandBuffer::HgiVkCommandBuffer(
    HgiVkDevice* device,
    HgiVkCommandPool* commandPool,
    HgiVkCommandBufferUsage usage)
    : _device(device)
    , _commandPool(commandPool)
    , _usage(usage)
    , _isRecording(false)
    , _vkCommandBuffer(nullptr)
    , _vkTimeStampQueryPool(nullptr)
    , _timeQueriesReset(false)
{
    //
    // Create command buffer
    //
    VkCommandBufferAllocateInfo allocateInfo =
        {VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};

    allocateInfo.commandBufferCount = 1;
    allocateInfo.commandPool = _commandPool->GetVulkanCommandPool();
    allocateInfo.level = _usage==HgiVkCommandBufferUsagePrimary ?
        VK_COMMAND_BUFFER_LEVEL_PRIMARY :
        VK_COMMAND_BUFFER_LEVEL_SECONDARY;

    TF_VERIFY(
        vkAllocateCommandBuffers(
            device->GetVulkanDevice(),
            &allocateInfo,
            &_vkCommandBuffer) == VK_SUCCESS
    );

    //
    // TimeStamp query pool
    //
    if (_device->GetDeviceSupportTimeStamps()) {
        _timeQueries.reserve(HGIVK_MAX_TIMESTAMPS/2);

        VkQueryPoolCreateInfo queryPoolInfo =
            {VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO};

        queryPoolInfo.queryType = VK_QUERY_TYPE_TIMESTAMP;
        queryPoolInfo.queryCount = HGIVK_MAX_TIMESTAMPS;
        TF_VERIFY(
            vkCreateQueryPool(
                _device->GetVulkanDevice(),
                &queryPoolInfo,
                HgiVkAllocator(),
                &_vkTimeStampQueryPool) == VK_SUCCESS
        );
    }
}

HgiVkCommandBuffer::~HgiVkCommandBuffer()
{
    // Prevent vulkan validation from warning that we are destroying a
    // command buffer that is being recorded into.
    EndRecording();

    if (_vkTimeStampQueryPool) {
       vkDestroyQueryPool(
            _device->GetVulkanDevice(),
            _vkTimeStampQueryPool,
            HgiVkAllocator());
    }

    vkFreeCommandBuffers(
        _device->GetVulkanDevice(),
        _commandPool->GetVulkanCommandPool(),
        1, // command buffer cnt
        &_vkCommandBuffer);
}

void
HgiVkCommandBuffer::SetRenderPass(HgiVkRenderPass* rp)
{
    _vkInheritanceInfo =
        {VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_INFO};
    _vkInheritanceInfo.renderPass = rp->GetVulkanRenderPass();
    _vkInheritanceInfo.framebuffer = rp->GetVulkanFramebuffer();
}

void
HgiVkCommandBuffer::EndRecording()
{
    if (_isRecording) {
        TF_VERIFY(
            vkEndCommandBuffer(_vkCommandBuffer) == VK_SUCCESS
        );

        // Next frame this command buffer may be used by an entirely different
        // encoder, so clear the render pass info for secondary command buffers.
        _vkInheritanceInfo.renderPass = nullptr;
        _vkInheritanceInfo.framebuffer = nullptr;

        _isRecording = false;
        _timeQueriesReset = false;
    }
}

bool
HgiVkCommandBuffer::IsRecording() const
{
    return _isRecording;
}

VkCommandBuffer
HgiVkCommandBuffer::GetCommandBufferForRecoding()
{
    _BeginRecording();
    return _vkCommandBuffer;
}

VkCommandBuffer
HgiVkCommandBuffer::GetVulkanCommandBuffer() const
{
    return _vkCommandBuffer;
}

void
HgiVkCommandBuffer::SetDebugName(const char* name)
{
    std::string debugLabel = "Command Buffer " + std::string(name);
    HgiVkSetDebugName(
        _device,
        (uint64_t)_vkCommandBuffer,
        VK_DEBUG_REPORT_OBJECT_TYPE_COMMAND_BUFFER_EXT,
        debugLabel.c_str());

    if (_vkTimeStampQueryPool) {
        debugLabel = "Query Pool " + std::string(name);
        HgiVkSetDebugName(
            _device,
            (uint64_t)_vkTimeStampQueryPool,
            VK_DEBUG_REPORT_OBJECT_TYPE_QUERY_POOL_EXT,
            debugLabel.c_str());
    }
}

void
HgiVkCommandBuffer::PushTimeQuery(const char* name)
{
    if (!_vkTimeStampQueryPool || !_timeQueriesReset) return;

    if (!TF_VERIFY(_timeQueries.size() < HGIVK_MAX_TIMESTAMPS/2,
                   "TimeStamp overflow")) {
        return;
   }

   _BeginRecording();

    // Reserve two time stamps, one for start, one for end.
    HgiTimeQuery query;
    query.beginStamp = _timeQueries.size() * 2;
    query.endStamp = 0; // zero until stamp is ended!
    query.name = name;

    // XXX to more precisely measure the performance of e.g. a compute shader it
    // could be interesting to use COMPUTE_SHADER_BIT here or other shader bits.
    // PopTimeQuery would also use COMPUTE_SHADER_BIT.

    vkCmdWriteTimestamp(
        _vkCommandBuffer,
        VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
        _vkTimeStampQueryPool,
        query.beginStamp);

    _timeQueries.emplace_back(std::move(query));
}

void
HgiVkCommandBuffer::PopTimeQuery()
{
    if (!_vkTimeStampQueryPool || !_timeQueriesReset) return;

    if (!TF_VERIFY(!_timeQueries.empty(), "Timestamp stack invalid")) return;

    // Find the last pushed, but not popped stamp.
    // We know a query not popped yet if its 'endStamp' is set to zero.
    HgiTimeQuery* query = nullptr;

    for (size_t i=_timeQueries.size(); i-- > 0;) {
        if (_timeQueries[i].endStamp == 0) {
            query = &_timeQueries[i];
            break;
        }
    }

    if (TF_VERIFY(query)) {
        // flag this time query as 'popped'
        query->endStamp = query->beginStamp + 1;

        vkCmdWriteTimestamp(
            _vkCommandBuffer,
            VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
            _vkTimeStampQueryPool,
            query->endStamp);
    }
}

HgiTimeQueryVector const&
HgiVkCommandBuffer::GetTimeQueries()
{
    if (!_vkTimeStampQueryPool) return _timeQueries;

    VkPhysicalDeviceProperties p = _device->GetVulkanPhysicalDeviceProperties();
    float toNanoSec = p.limits.timestampPeriod;

    uint32_t data[2] = {};

    // XXX On APPLE (MoltenVk) I believe the time queries happen at cmd buf
    // boundaries. This means both our begin and end stamps will have the
    // same value, which will produces 0.0.
    // https://github.com/KhronosGroup/MoltenVK/issues/520

    for (HgiTimeQuery& query : _timeQueries) {
        vkGetQueryPoolResults(
            _device->GetVulkanDevice(),
            _vkTimeStampQueryPool,
            query.beginStamp,
            2,
            sizeof(uint32_t) * 2,
            data,
            sizeof(uint32_t),
            VK_QUERY_RESULT_WAIT_BIT);

        uint32_t diff = data[1] - data[0];
        query.nanoSeconds = diff * p.limits.timestampPeriod;
    }

    return _timeQueries;
}

void
HgiVkCommandBuffer::ResetTimeQueries(HgiVkCommandBuffer* cb)
{
    if (!_vkTimeStampQueryPool) return;

    // Reset time stamps - Timestamps must be reset before they can be used.
    // In vulkan 1.0 this must be recorded in a command buffer (how fun!).
    // We use the provided (primary) command buffer, because we don't want to
    // start each command buffer (incl secondary cmd bufs) if they aren't needed
    // by a thread. So we record all resets into one single command buffer.

    // XXX VK_KHR_performance_query can do it on a device level, which would fit
    // our design much better. But, for now, it is not that well supported.

    vkCmdResetQueryPool(
        cb->GetCommandBufferForRecoding(), // Don't use internal cmd buf!
        _vkTimeStampQueryPool,
        0, // first time stamp
        HGIVK_MAX_TIMESTAMPS);

    _timeQueries.clear();
    _timeQueriesReset = true;
}

void
HgiVkCommandBuffer::_BeginRecording()
{
    if (!_isRecording) {

        VkCommandBufferBeginInfo beginInfo =
            {VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
        beginInfo.flags |= VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

        if (_usage == HgiVkCommandBufferUsageSecondaryRenderPass) {
            beginInfo.flags |= VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT;
            beginInfo.pInheritanceInfo = &_vkInheritanceInfo;
        }

        // Begin recording
        TF_VERIFY(
            vkBeginCommandBuffer(_vkCommandBuffer, &beginInfo) == VK_SUCCESS
        );

        _isRecording = true;
    }
}

PXR_NAMESPACE_CLOSE_SCOPE
