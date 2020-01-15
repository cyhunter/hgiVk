#include "pxr/base/tf/diagnostic.h"

#include "pxr/imaging/hgiVk/buffer.h"
#include "pxr/imaging/hgiVk/garbageCollector.h"
#include "pxr/imaging/hgiVk/hgi.h"
#include "pxr/imaging/hgiVk/pipeline.h"
#include "pxr/imaging/hgiVk/renderPass.h"
#include "pxr/imaging/hgiVk/resourceBindings.h"
#include "pxr/imaging/hgiVk/shaderFunction.h"
#include "pxr/imaging/hgiVk/shaderProgram.h"
#include "pxr/imaging/hgiVk/surface.h"
#include "pxr/imaging/hgiVk/swapchain.h"
#include "pxr/imaging/hgiVk/texture.h"
#include "pxr/imaging/hgiVk/vulkan.h"

PXR_NAMESPACE_OPEN_SCOPE

thread_local uint16_t _HgiVkgcThreadLocalIndex = 0;
thread_local uint64_t _HgiVkgcThreadLocalFrame = ~0ull;


HgiVkGarbageCollector::HgiVkGarbageCollector()
    : _numUsedExpired(0)
    , _frame(~0ull)
    , _frameStarted(false)
{
}

HgiVkGarbageCollector::~HgiVkGarbageCollector()
{
    DestroyGarbage(~0ull);
}

void
HgiVkGarbageCollector::ScheduleObjectDestruction(
    HgiVkObject const& obj)
{
    // First time thread is used in new frame, reserve index into vector.
    if (_HgiVkgcThreadLocalFrame != _frame) {
        _HgiVkgcThreadLocalFrame = _frame;
        _HgiVkgcThreadLocalIndex = _numUsedExpired.fetch_add(1);
    }

    if (_HgiVkgcThreadLocalIndex >= _expiredVulkanObjects.size()) {
        TF_CODING_ERROR("GC numThreads > HgiVk::GetThreadCount");
        _HgiVkgcThreadLocalIndex = 0;
    }

    _expiredVulkanObjects[_HgiVkgcThreadLocalIndex].emplace_back(obj);
}

void
HgiVkGarbageCollector::DestroyGarbage(uint64_t frame)
{
    // Change the frame counter. This will let each thread know that they
    // must re-initializes themselves the next time the thread wants to schedule
    // an object for destruction.
    _frame = frame;

    // Loop the expired objects for each thread
    for (VkObjectVector const& vec : _expiredVulkanObjects) {
        // Loop each object and destroy it
        for (HgiVkObject const& obj : vec) {
            switch(obj.type) {
                default: TF_CODING_ERROR("Missing destroy for hgiVk object");

                case HgiVkObjectTypeTexture: {
                    HgiVkTexture* t = obj.texture;
                    delete t;
                    break;
                }
                case HgiVkObjectTypeBuffer: {
                    HgiVkBuffer* b = obj.buffer;
                    delete b;
                    break;
                }
                case HgiVkObjectTypeRenderPass: {
                    HgiVkRenderPass* r = obj.renderPass;
                    delete r;
                    break;
                }
                case HgiVkObjectTypePipeline: {
                    HgiVkPipeline* p = obj.pipeline;
                    delete p;
                    break;
                }
                case HgiVkObjectTypeResourceBindings: {
                    HgiVkResourceBindings* r = obj.resourceBindings;
                    delete r;
                    break;
                }
                case HgiVkObjectTypeShaderFunction: {
                    HgiVkShaderFunction* s = obj.shaderFunction;
                    delete s;
                    break;
                }
                case HgiVkObjectTypeShaderProgram: {
                    HgiVkShaderProgram* s = obj.shaderProgram;
                    delete s;
                    break;
                }
               case HgiVkObjectTypeSurface: {
                    HgiVkSurface* s = obj.surface;
                    delete s;
                    break;
                }
               case HgiVkObjectTypeSwapchain: {
                    HgiVkSwapchain* s = obj.swapchain;
                    delete s;
                    break;
                }
            }
        }
    }

    _expiredVulkanObjects.clear();
    _expiredVulkanObjects.shrink_to_fit();
    _numUsedExpired.store(0);

    // Make sure we have enough room for each thread, just in case the thread
    // count has changed since last frame.
    uint32_t numThreads = HgiVk::GetThreadCount();
    _expiredVulkanObjects.resize(numThreads);
}


PXR_NAMESPACE_CLOSE_SCOPE
