#ifndef PXR_IMAGING_HGIVK_GRAPHICS_ENCODER_H
#define PXR_IMAGING_HGIVK_GRAPHICS_ENCODER_H

#include <cstdint>

#include "pxr/pxr.h"
#include "pxr/base/gf/vec4i.h"

#include "pxr/imaging/hgi/graphicsEncoder.h"
#include "pxr/imaging/hgi/pipeline.h"
#include "pxr/imaging/hgi/resourceBindings.h"

#include "pxr/imaging/hgiVk/api.h"

PXR_NAMESPACE_OPEN_SCOPE

class HgiVkCommandBuffer;
class HgiVkDevice;
class HgiVkRenderPass;
struct HgiGraphicsEncoderDesc;


/// \class HgiVkGraphicsEncoder
///
/// Vulkan implementation of HgiGraphicsEncoder.
///
class HgiVkGraphicsEncoder final : public HgiGraphicsEncoder
{
public:
    /// Constructor for recording in primary command buffer
    HGIVK_API
    HgiVkGraphicsEncoder(
        HgiVkDevice* device,
        HgiVkCommandBuffer* cb,
        HgiGraphicsEncoderDesc const& desc);

    /// Constructor for parallel recording into secondary command buffer
    HGIVK_API
    HgiVkGraphicsEncoder(
        HgiVkDevice* device,
        HgiVkCommandBuffer* cb,
        HgiVkRenderPass* renderPass);

    HGIVK_API
    virtual ~HgiVkGraphicsEncoder();

    HGIVK_API
    void EndEncoding() override;

    HGIVK_API
    void SetViewport(GfVec4i const& vp) override;

    HGIVK_API
    void SetScissor(GfVec4i const& sc) override;

    HGIVK_API
    void BindPipeline(HgiPipelineHandle pipeline) override;

    HGIVK_API
    void BindResources(HgiResourceBindingsHandle resources) override;

    HGIVK_API
    void BindVertexBuffers(HgiBufferHandleVector const& vertexBuffers) override;

    HGIVK_API
    void SetConstantValues(
        HgiResourceBindingsHandle resources,
        HgiShaderStage stages,
        uint32_t byteOffset,
        uint32_t byteSize,
        const void* data) override;

    HGIVK_API
    void DrawIndexed(
        HgiBufferHandle const& indexBuffer,
        uint32_t indexCount,
        uint32_t indexBufferByteOffset,
        uint32_t firstIndex,
        uint32_t vertexOffset,
        uint32_t instanceCount,
        uint32_t firstInstance) override;

    HGIVK_API
    void PushDebugGroup(const char* label) override;

    HGIVK_API
    void PopDebugGroup() override;

    HGIVK_API
    void PushTimeQuery(const char* name) override;

    HGIVK_API
    void PopTimeQuery() override;

private:
    HgiVkGraphicsEncoder() = delete;
    HgiVkGraphicsEncoder & operator=(const HgiVkGraphicsEncoder&) = delete;
    HgiVkGraphicsEncoder(const HgiVkGraphicsEncoder&) = delete;

private:
    HgiVkDevice* _device;
    HgiVkCommandBuffer* _commandBuffer;
    HgiVkRenderPass* _renderPass;
    bool _isParallelEncoder;
    bool _isRecording;

    // Encoder is used only one frame so storing multi-frame state on encoder
    // will not survive.
};

PXR_NAMESPACE_CLOSE_SCOPE

#endif
