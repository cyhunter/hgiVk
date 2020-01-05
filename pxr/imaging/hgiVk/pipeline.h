#ifndef PXR_IMAGING_HGIVK_PIPELINE_H
#define PXR_IMAGING_HGIVK_PIPELINE_H

#include <vector>

#include "pxr/pxr.h"
#include "pxr/imaging/hgi/graphicsEncoderDesc.h"
#include "pxr/imaging/hgi/pipeline.h"

#include "pxr/imaging/hgiVk/api.h"
#include "pxr/imaging/hgiVk/vulkan.h"


PXR_NAMESPACE_OPEN_SCOPE

class HgiVkDevice;
class HgiVkCommandBuffer;
class HgiVkRenderPass;


/// \class HgiVkPipeline
///
/// Vulkan implementation of HgiPipeline.
///
class HgiVkPipeline final : public HgiPipeline {
public:
    HGIVK_API
    HgiVkPipeline(
        HgiVkDevice* device,
        HgiPipelineDesc const& desc);

    HGIVK_API
    virtual ~HgiVkPipeline();

    /// Bind this pipeline to GPU.
    HGIVK_API
    void BindPipeline(
        HgiVkCommandBuffer* cb,
        HgiVkRenderPass* rp);

    // Create pipeline object for _descriptor and render pass.
    VkPipeline AcquirePipeline(HgiVkRenderPass* rp);

private:
    HgiVkPipeline() = delete;
    HgiVkPipeline & operator=(const HgiVkPipeline&) = delete;
    HgiVkPipeline(const HgiVkPipeline&) = delete;

    // In Vulkan pipelines require compatibility with render passes.
    // In Hgi we use gfx encoders instead of render passes.
    // This struct stores the gfx descriptor the pipeline was made for.
    struct _Pipeline {
        HgiGraphicsEncoderDesc desc;
        VkPipeline vkPipeline;
    };

private:
    HgiVkDevice* _device;
    HgiPipelineDesc _descriptor;
    std::vector<_Pipeline> _pipelines;
    VkPrimitiveTopology _vkTopology;
};


PXR_NAMESPACE_CLOSE_SCOPE

#endif
