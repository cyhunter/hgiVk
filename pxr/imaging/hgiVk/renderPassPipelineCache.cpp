#include <algorithm>

#include "pxr/base/tf/diagnostic.h"

#include "pxr/imaging/hgi/graphicsEncoderDesc.h"

#include "pxr/imaging/hgiVk/device.h"
#include "pxr/imaging/hgiVk/hgi.h"
#include "pxr/imaging/hgiVk/object.h"
#include "pxr/imaging/hgiVk/pipeline.h"
#include "pxr/imaging/hgiVk/renderPass.h"
#include "pxr/imaging/hgiVk/renderPassPipelineCache.h"
#include "pxr/imaging/hgiVk/texture.h"


PXR_NAMESPACE_OPEN_SCOPE

thread_local uint16_t _HgiVkrpcThreadLocalIndex = 0;
thread_local uint64_t _HgiVkrpcThreadLocalFrame = ~0ull;


struct HgiVkRenderPassCacheItem {
    ~HgiVkRenderPassCacheItem() {
        HgiVkObject object;
        object.type = HgiVkObjectTypeRenderPass;
        object.renderPass = renderPass;
        device->DestroyObject(object);
    }

    HgiVkDevice* device = nullptr;
    HgiGraphicsEncoderDesc descriptor;
    HgiVkRenderPass* renderPass = nullptr;
};

struct HgiVkRenderPassSort {
    inline bool operator() (
        const HgiVkRenderPassCacheItem* a,
        const HgiVkRenderPassCacheItem* b) {
        // Note we sort biggest to smallest frame number so that the oldest
        // items are at the back of vector for efficient pop_back.
        return (a->renderPass->GetLastUsedFrame() >
                b->renderPass->GetLastUsedFrame());
    }
};

static HgiVkRenderPassCacheItem*
_CreateRenderPassCacheItem(
    HgiVkDevice* device,
    const HgiGraphicsEncoderDesc& desc)
{
    HgiVkRenderPassCacheItem* rci = new HgiVkRenderPassCacheItem();
    rci->device = device;
    rci->descriptor = desc;
    rci->renderPass = new HgiVkRenderPass(device, desc);
    return rci;
}

static void
_DestroyRenderPassCacheItem(HgiVkRenderPassCacheItem* rci)
{
    delete rci;
}

static bool
_CompareHgiGraphicsEncoderDesc(
    HgiGraphicsEncoderDesc const& newDesc,
    HgiVkRenderPassCacheItem const& cacheItem)
{
    // Comparing descriptors compares the properties and texture handles between
    // the two descriptors. However it is pretty likely that a texture handle
    // was deleted and later we get the same texture handle (ptr) for a
    // different, new texture. Handles are pointers and it is up to the system
    // to decide when it re-uses heap memory. If the handles do not match, we
    // can safely say the render pass is not a match.
    // However, if the descriptors match, we still need to do a deeper
    // validation by comparing the vkImage handles.

    if (newDesc != cacheItem.descriptor) return false;

    HgiVkRenderPass* renderPass = cacheItem.renderPass;
    std::vector<VkImageView> const& imageViews = renderPass->GetImageViews();

    // Sort the attachments the same way the render pass had sorted its.
    // That way we can compare the imageViews of the new descriptor and the
    // renderpass in the correct order.
    HgiAttachmentDescConstPtrVector attachmentL =
        HgiVkRenderPass::GetCombinedAttachments(newDesc);

    for (size_t i=0; i<attachmentL.size(); i++) {
        VkImageView renderPassView = imageViews[i];

        HgiAttachmentDesc const* aL = attachmentL[i];
        HgiVkTexture* tex = static_cast<HgiVkTexture*>(aL->texture);

        if (renderPassView != tex->GetImageView()) return false;
    }

    return true;
}

HgiVkRenderPassPipelineCache::HgiVkRenderPassPipelineCache()
    : _frame(~0ull)
    , _frameStarted(false)
    , _nextThreadLocalIndex(0)
{
}

HgiVkRenderPassPipelineCache::~HgiVkRenderPassPipelineCache()
{
    Clear();
}

HgiVkRenderPass*
HgiVkRenderPassPipelineCache::AcquireRenderPass(
    HgiVkDevice* device,
    HgiGraphicsEncoderDesc const& desc)
{
    // First look in read-only cache for existing matching renderpass.
    // We cannot add new items in _renderPassReadOnlyCache, because this a
    // multi-threaded call and the cache is not thread-safe.
    // We don't want to slow things down with a mutex because after the first
    // frame we will usually find the render pass in this (read-only) cache.

    for (size_t i=0; i<_renderPassReadOnlyCache.size(); i++) {
        HgiVkRenderPassCacheItem* item = _renderPassReadOnlyCache[i];
        if (_CompareHgiGraphicsEncoderDesc(desc, *item)) {
            // A vulkan render pass cannot span across multiple command buffers.
            // It must begin and end in the same command buffer.
            // By testing with AcquireRenderPass we prevent multiple threads
            // from using the same render pass during parallel recording.
            bool available = item->renderPass->AcquireRenderPass();
            if (available) {
                return item->renderPass;
            }
        }
    }

    // Acquire this thread's index into the thread local passes / pipelines.

    if (_HgiVkrpcThreadLocalFrame != _frame) {
        _HgiVkrpcThreadLocalIndex = _nextThreadLocalIndex.fetch_add(1);
        _HgiVkrpcThreadLocalFrame = _frame;
    }

    // If we didn't find the render pass in the global cache, look for it in
    // our thread_local vector to see if we already created a matching
    // render pass this frame.

    if (_HgiVkrpcThreadLocalIndex >= _threadRenderPasses.size()) {
        TF_CODING_ERROR("rpc numThreads > HgiVk::GetThreadCount");
        _HgiVkrpcThreadLocalIndex = 0;
    }

    HgiVkRenderPassCacheVec& pv= _threadRenderPasses[_HgiVkrpcThreadLocalIndex];
    for (size_t i=0; i<pv.size(); i++) {
        HgiVkRenderPassCacheItem* item = pv[i];
        if (_CompareHgiGraphicsEncoderDesc(desc, *item)) {
            return item->renderPass;
        }
    }

    // If we found nothing, create a new render pass in thread_local vector.
    // This new render pass will get merged into the renderPassCache at the
    // end of the frame.
    HgiVkRenderPassCacheItem* pci = _CreateRenderPassCacheItem(device, desc);
    pv.push_back(pci);
    return pci->renderPass;
}

void
HgiVkRenderPassPipelineCache::BeginFrame(uint64_t frame)
{
    if (_frameStarted) return;
    _frameStarted = true;

    // Change the frame counter. This will let each thread know that they
    // must re-initializes themselves the next time a thread uses the locals.
    _frame = frame;

    // Ensure the thread_local vectors have enough room for each thread.
    uint32_t numThreads = HgiVk::GetThreadCount();

    if (_threadRenderPasses.size() != numThreads) {
        _threadRenderPasses.resize(numThreads);
    }
}

void
HgiVkRenderPassPipelineCache::EndFrame()
{
    const size_t descriptorLRUsize = 32;

    HgiVkRenderPassCacheVec& passCache = _renderPassReadOnlyCache;

    // Merge all thread_local passes / pipelines into read-only cache.
    // We sort to ensure the newly created items are last to exit cache.
    //
    // It is possible (though rare) that two threads haved created the exact
    // same renderpass or pipeline within one frame. We make no attempt here to
    // erase the dups, because we consider these lightweight objects
    // (textures/render-targets are not duplicated, only their description).
    //
    // These dups will eventually be removed from the cache when
    // the cache size limit is reached.

    for (HgiVkRenderPassCacheVec& v : _threadRenderPasses) {
        passCache.insert(passCache.end(), v.begin(), v.end());
        v.clear();
        v.shrink_to_fit();
    }
    std::sort(passCache.begin(), passCache.end());

    // If we reached the max size of the cache remove the oldest items.
    if (passCache.size() > descriptorLRUsize) {
        std::sort(passCache.begin(), passCache.end(), HgiVkRenderPassSort());

        while (passCache.size() > descriptorLRUsize) {
            _DestroyRenderPassCacheItem(passCache.back());
            passCache.pop_back();
        }
    }

    // Next frame each thread should acquire a new index into thread_local vecs.
    _nextThreadLocalIndex.store(0);
    _frameStarted = false;
}

void
HgiVkRenderPassPipelineCache::Clear()
{
    // Make sure any thread_local render passes are merged into read-only cache.
    EndFrame();

    // Destroy all render passes
    for (HgiVkRenderPassCacheItem* rci : _renderPassReadOnlyCache) {
        _DestroyRenderPassCacheItem(rci);
    }
    _renderPassReadOnlyCache.clear();
}

PXR_NAMESPACE_CLOSE_SCOPE
