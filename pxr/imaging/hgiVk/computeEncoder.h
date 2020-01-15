#ifndef PXR_IMAGING_HGIVK_COMPUTE_ENCODER_H
#define PXR_IMAGING_HGIVK_COMPUTE_ENCODER_H

#include "pxr/pxr.h"
#include "pxr/imaging/hgi/computeEncoder.h"
#include "pxr/imaging/hgi/pipeline.h"
#include "pxr/imaging/hgi/resourceBindings.h"

#include "pxr/imaging/hgiVk/api.h"


PXR_NAMESPACE_OPEN_SCOPE

class HgiVkCommandBuffer;
class HgiVkDevice;


/// \class HgiVkComputeEncoder
///
/// Vulkan implementation of HgiComputeEncoder.
///
class HgiVkComputeEncoder final : public HgiComputeEncoder
{
public:
    HGIVK_API
    HgiVkComputeEncoder(
        HgiVkDevice* device,
        HgiVkCommandBuffer* cb);

    HGIVK_API
    virtual ~HgiVkComputeEncoder();

    HGIVK_API
    void EndEncoding() override;

    HGIVK_API
    void BindPipeline(HgiPipelineHandle pipeline) override;

    HGIVK_API
    void BindResources(HgiResourceBindingsHandle resources) override;

    HGIVK_API
    void Dispatch(
        uint32_t threadGrpCntX,
        uint32_t threadGrpCntY,
        uint32_t threadGrpCntZ) override;

    /// Push a debug marker onto the encoder.
    HGI_API
    void PushDebugGroup(const char* label) override;

    /// Pop the lastest debug marker off encoder.
    HGI_API
    void PopDebugGroup() override;

private:
    HgiVkComputeEncoder() = delete;
    HgiVkComputeEncoder & operator=(const HgiVkComputeEncoder&) = delete;
    HgiVkComputeEncoder(const HgiVkComputeEncoder&) = delete;

private:
    HgiVkDevice* _device;
    HgiVkCommandBuffer* _commandBuffer;
    bool _isRecording;

    // Encoder is used only one frame so storing multi-frame state on encoder
    // will not survive.
};


PXR_NAMESPACE_CLOSE_SCOPE

#endif
