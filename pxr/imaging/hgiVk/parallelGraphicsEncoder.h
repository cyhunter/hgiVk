#ifndef PXR_IMAGING_HGIVK_PARALLEL_GRAPHICS_ENCODER_H
#define PXR_IMAGING_HGIVK_PARALLEL_GRAPHICS_ENCODER_H

#include <atomic>
#include <cstdint>

#include "pxr/pxr.h"
#include "pxr/imaging/hgi/parallelGraphicsEncoder.h"
#include "pxr/imaging/hgi/graphicsEncoder.h"

#include "pxr/imaging/hgiVk/api.h"

PXR_NAMESPACE_OPEN_SCOPE

class HgiVkCommandBuffer;
class HgiVkDevice;
class HgiVkRenderPass;
struct HgiGraphicsEncoderDesc;


/// \class HgiVkParallelGraphicsEncoder
///
/// Vulkan implementation of HgiParallelGraphicsEncoder.
///
class HgiVkParallelGraphicsEncoder final : public HgiParallelGraphicsEncoder
{
public:
    HGIVK_API
    HgiVkParallelGraphicsEncoder(
        const char* debugName,
        HgiVkDevice* device,
        HgiVkCommandBuffer* primaryCB,
        HgiGraphicsEncoderDesc const& desc,
        HgiPipelineHandle pipeline);

    HGIVK_API
    virtual ~HgiVkParallelGraphicsEncoder();

    HGIVK_API
    HgiGraphicsEncoderUniquePtr CreateGraphicsEncoder() override;

    HGIVK_API
    void EndEncoding() override;

private:
    HgiVkParallelGraphicsEncoder() = delete;
    HgiVkParallelGraphicsEncoder & operator=(
        const HgiVkParallelGraphicsEncoder&) = delete;
    HgiVkParallelGraphicsEncoder(
        const HgiVkParallelGraphicsEncoder&) = delete;

private:
    HgiVkDevice* _device;
    HgiVkCommandBuffer* _primaryCommandBuffer;
    HgiVkRenderPass* _renderPass;
    bool _isRecording;
    bool _isDebugging;
    size_t _cmdBufBlockId;

    // Encoder is used only one frame so storing multi-frame state on encoder
    // will not survive.
};

PXR_NAMESPACE_CLOSE_SCOPE

#endif
