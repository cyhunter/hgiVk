#ifndef PXR_IMAGING_HGIVK_RENDERPASS_PIPELINE_CACHE_H
#define PXR_IMAGING_HGIVK_RENDERPASS_PIPELINE_CACHE_H

#include <atomic>
#include <vector>

#include "pxr/pxr.h"
#include "pxr/imaging/hgi/graphicsEncoderDesc.h"

#include "pxr/imaging/hgi/pipeline.h"

#include "pxr/imaging/hgiVk/api.h"


PXR_NAMESPACE_OPEN_SCOPE

class HgiVkDevice;
class HgiVkPipeline;
class HgiVkRenderPass;


typedef std::vector<struct HgiVkRenderPassCacheItem*> HgiVkRenderPassCacheVec;
typedef std::vector<HgiVkRenderPassCacheVec> HgiVkRenderPassThreadLocalVec;

typedef std::vector<struct HgiVkPipelineCacheItem*> HgiVkPipelineCacheVec;
typedef std::vector<HgiVkPipelineCacheVec> HgiVkPipelineThreadLocalVec;


/// \class HgiVkRenderPassPipelineCache
///
/// Stores a cache of render passes objects.
///
/// Render passes are not directly managed by the Hgi client. Instead the
/// client request 'Encoders' and 'Pipelines' via descriptors.
/// When a pipeline is bound on the graphics encoder, this cache is used to
/// acquire a vulkan render pass that is compatible with encoder and pipeline.
///
class HgiVkRenderPassPipelineCache final
{
public:
    HGIVK_API
    HgiVkRenderPassPipelineCache();

    HGIVK_API
    virtual ~HgiVkRenderPassPipelineCache();

    /// Returns a render pass for the provided descriptor.
    /// Will create a new render pass if non existed that matched descriptor.
    /// The lifetime of the render pass is internally managed. You should not
    /// delete or hold onto the render pass pointer at the end of the frame.
    HGIVK_API
    HgiVkRenderPass* AcquireRenderPass(
        HgiVkDevice* device,
        HgiGraphicsEncoderDesc const& desc);

    /// Reset thread local render pass and pipeline caches.
    HGIVK_API
    void BeginFrame(uint64_t frame);

    /// Commits all newly created (thread local) render passes and pipelines
    /// into the cache so they may be re-used next frame.
    HGIVK_API
    void EndFrame();

    /// Clears all render pass from cache.
    /// This initiates destruction of all render passes and should generally
    /// only be called when the device is being destroyed.
    HGIVK_API
    void Clear();

private:
    HgiVkRenderPassPipelineCache & operator= (
        const HgiVkRenderPassPipelineCache&) = delete;
    HgiVkRenderPassPipelineCache(const HgiVkRenderPassPipelineCache&) = delete;

private:
    uint64_t _frame;
    bool _frameStarted;

    std::atomic<uint16_t> _nextThreadLocalIndex;

    HgiVkRenderPassCacheVec _renderPassReadOnlyCache;
    HgiVkRenderPassThreadLocalVec _threadRenderPasses;
};


PXR_NAMESPACE_CLOSE_SCOPE

#endif
