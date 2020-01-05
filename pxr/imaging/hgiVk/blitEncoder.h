#ifndef PXR_IMAGING_HGIVK_BLIT_ENCODER_H
#define PXR_IMAGING_HGIVK_BLIT_ENCODER_H

#include "pxr/pxr.h"
#include "pxr/imaging/hgiVk/api.h"
#include "pxr/imaging/hgi/blitEncoder.h"

PXR_NAMESPACE_OPEN_SCOPE

class HgiVkCommandBuffer;
class HgiVkDevice;

/// \class HgiVkBlitEncoder
///
/// Vulkan implementation of HgiBlitEncoder.
///
class HgiVkBlitEncoder final : public HgiBlitEncoder
{
public:
    HGIVK_API
    HgiVkBlitEncoder(
        HgiVkDevice* device,
        HgiVkCommandBuffer* cmdBuf);

    HGIVK_API
    virtual ~HgiVkBlitEncoder();

    HGIVK_API
    void EndEncoding() override;

    HGIVK_API
    void PushDebugGroup(const char* label) override;

    HGIVK_API
    void PopDebugGroup() override;

    HGIVK_API
    void CopyTextureGpuToCpu(HgiTextureGpuToCpuOp const& copyOp) override;

    HGIVK_API
    void ResolveImage(HgiResolveImageOp const& resolveOp) override;

private:
    HgiVkBlitEncoder() = delete;
    HgiVkBlitEncoder & operator=(const HgiVkBlitEncoder&) = delete;
    HgiVkBlitEncoder(const HgiVkBlitEncoder&) = delete;

private:
    HgiVkDevice* _device;
    HgiVkCommandBuffer* _commandBuffer;
    bool _isRecording;

    // Encoder is used only one frame so storing multi-frame state on encoder
    // will not survive.
};

PXR_NAMESPACE_CLOSE_SCOPE

#endif
