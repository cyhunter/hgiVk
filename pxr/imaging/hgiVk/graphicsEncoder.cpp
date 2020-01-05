#include "pxr/imaging/hgi/graphicsEncoderDesc.h"

#include "pxr/imaging/hgiVk/buffer.h"
#include "pxr/imaging/hgiVk/commandBuffer.h"
#include "pxr/imaging/hgiVk/conversions.h"
#include "pxr/imaging/hgiVk/device.h"
#include "pxr/imaging/hgiVk/diagnostic.h"
#include "pxr/imaging/hgiVk/graphicsEncoder.h"
#include "pxr/imaging/hgiVk/pipeline.h"
#include "pxr/imaging/hgiVk/renderPass.h"
#include "pxr/imaging/hgiVk/resourceBindings.h"
#include "pxr/imaging/hgiVk/texture.h"
#include "pxr/imaging/hgiVk/vulkan.h"

PXR_NAMESPACE_OPEN_SCOPE

HgiVkGraphicsEncoder::HgiVkGraphicsEncoder(
    HgiVkDevice* device,
    HgiVkCommandBuffer* cb,
    HgiGraphicsEncoderDesc const& desc)
    : HgiGraphicsEncoder()
    , _device(device)
    , _commandBuffer(cb)
    , _renderPass(nullptr)
    , _isParallelEncoder(false)
    , _isRecording(true)
{
    _renderPass = device->AcquireRenderPass(desc);
    _renderPass->BeginRenderPass(_commandBuffer, _isParallelEncoder);
}

HgiVkGraphicsEncoder::HgiVkGraphicsEncoder(
    HgiVkDevice* device,
    HgiVkCommandBuffer* cb,
    HgiVkRenderPass* renderPass)
    : HgiGraphicsEncoder()
    , _device(device)
    , _commandBuffer(cb)
    , _renderPass(renderPass)
    , _isParallelEncoder(true)
    , _isRecording(true)
{
    // If this encoder is created via ParallelGraphicsEncoder we do not want to
    // begin the render pass. The parallel encoder will start and end the pass.
}

HgiVkGraphicsEncoder::~HgiVkGraphicsEncoder()
{
    if (_isRecording) {
        TF_WARN("Graphics Encoder: [%x] is missing an EndEncodig() call.");
        EndEncoding();
    }
}

void
HgiVkGraphicsEncoder::EndEncoding()
{
    if (!_isParallelEncoder) {
        _renderPass->EndRenderPass(_commandBuffer);
        _device->ReleaseRenderPass(_renderPass);
    }

    _commandBuffer = nullptr;
    _isRecording = false;
}

void
HgiVkGraphicsEncoder::SetViewport(GfVec4i const& vp)
{
    if (!TF_VERIFY(_isRecording && _commandBuffer)) return;

    float offsetX = (float) vp[0];
    float offsetY = (float) vp[1];
    float width = (float) vp[2];
    float height = (float) vp[3];

    // Flip viewport in Y-axis, because the vertex.y position is flipped
    // between opengl and vulkan. This also moves origin to bottom-left.
    // Requires VK_KHR_maintenance1 extension.

    // Alternatives are:
    // 1. Multiply projection by 'inverted Y and half Z' matrix:
    //    const GfMatrix4d clip(
    //        1.0,  0.0, 0.0, 0.0,
    //        0.0, -1.0, 0.0, 0.0,
    //        0.0,  0.0, 0.5, 0.0,
    //        0.0,  0.0, 0.5, 1.0);
    //    projection = /*clip **/ projection;
    //
    // 2. Adjust vertex position:
    //    gl_Position.z = (gl_Position.z + gl_Position.w) / 2.0;

    VkViewport viewport;
    viewport.x = offsetX;
    viewport.y = height - offsetY;
    viewport.width = width;
    viewport.height = -height;
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;

    vkCmdSetViewport(
        _commandBuffer->GetCommandBufferForRecoding(),
        0,
        1,
        &viewport);
}

void
HgiVkGraphicsEncoder::SetScissor(GfVec4i const& s)
{
    uint32_t w(s[2]);
    uint32_t h(s[3]);
    VkRect2D scissor = {{s[0], s[1]}, {w, h}};
    vkCmdSetScissor(
        _commandBuffer->GetCommandBufferForRecoding(),
        0,
        1,
        &scissor);
}

void
HgiVkGraphicsEncoder::PushDebugGroup(const char* label)
{
    if (!TF_VERIFY(_isRecording && _commandBuffer)) return;
    HgiVkBeginDebugMarker(_commandBuffer, label);
}

void
HgiVkGraphicsEncoder::PopDebugGroup()
{
    if (!TF_VERIFY(_isRecording && _commandBuffer)) return;
    HgiVkEndDebugMarker(_commandBuffer);
}

void
HgiVkGraphicsEncoder::BindPipeline(HgiPipelineHandle pipeline)
{
    if (HgiVkPipeline* p = static_cast<HgiVkPipeline*>(pipeline)) {
        p->BindPipeline(_commandBuffer, _renderPass);
    }
}

void
HgiVkGraphicsEncoder::BindResources(HgiResourceBindingsHandle res)
{
    if (HgiVkResourceBindings* r = static_cast<HgiVkResourceBindings*>(res)) {
        r->BindResources(_commandBuffer);
    }
}

void
HgiVkGraphicsEncoder::BindVertexBuffers(
    HgiBufferHandleVector const& vertexBuffers)
{
    std::vector<VkBuffer> buffers;
    std::vector<VkDeviceSize> bufferOffsets;

    for (HgiBufferHandle bufHandle : vertexBuffers) {
        HgiVkBuffer* buf = static_cast<HgiVkBuffer*>(bufHandle);
        VkBuffer vkBuf = buf->GetBuffer();
        if (vkBuf) {
            buffers.push_back(vkBuf);
            bufferOffsets.push_back(0);
        }
    }

    vkCmdBindVertexBuffers(
        _commandBuffer->GetCommandBufferForRecoding(),
        0, // first bindings
        buffers.size(),
        buffers.data(),
        bufferOffsets.data());
}

void
HgiVkGraphicsEncoder::SetConstantValues(
    HgiResourceBindingsHandle res,
    HgiShaderStage stages,
    uint32_t byteOffset,
    uint32_t byteSize,
    const void* data)
{
    HgiVkResourceBindings* r = static_cast<HgiVkResourceBindings*>(res);
    if (!TF_VERIFY(r)) return;

    vkCmdPushConstants(
        _commandBuffer->GetCommandBufferForRecoding(),
        r->GetPipelineLayout(),
        HgiVkConversions::GetShaderStages(stages),
        byteOffset,
        byteSize,
        data);
}

void
HgiVkGraphicsEncoder::DrawIndexed(
    HgiBufferHandle const& indexBuffer,
    uint32_t indexCount,
    uint32_t indexBufferByteOffset,
    uint32_t firstIndex,
    uint32_t vertexOffset,
    uint32_t instanceCount,
    uint32_t firstInstance)
{
    TF_VERIFY(instanceCount>0);

    HgiVkBuffer* vkIndexBuf = static_cast<HgiVkBuffer*>(indexBuffer);
    HgiBufferDesc const& indexDesc = vkIndexBuf->GetDescriptor();

    VkIndexType indexType = (indexDesc.usage & HgiBufferUsageIndex16) ?
        VK_INDEX_TYPE_UINT16 : VK_INDEX_TYPE_UINT32;

    vkCmdBindIndexBuffer(
        _commandBuffer->GetCommandBufferForRecoding(),
        vkIndexBuf->GetBuffer(),
        indexBufferByteOffset,
        indexType);

    vkCmdDrawIndexed(
        _commandBuffer->GetCommandBufferForRecoding(),
        indexCount,
        instanceCount,
        firstIndex,
        vertexOffset,
        firstInstance);
}

// XXX Draw calls could also be build up in a buffer and submitted to gpu, see:
// vkCmdDrawIndexedIndirect and vkCmdDrawIndexedIndirectCount

PXR_NAMESPACE_CLOSE_SCOPE
