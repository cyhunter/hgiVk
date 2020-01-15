#ifndef PXR_IMAGING_HGIVK_HGIVK_H
#define PXR_IMAGING_HGIVK_HGIVK_H

#include <atomic>
#include <vector>

#include "pxr/pxr.h"
#include "pxr/imaging/hgi/enums.h"
#include "pxr/imaging/hgi/hgi.h"
#include "pxr/imaging/hgiVk/api.h"


PXR_NAMESPACE_OPEN_SCOPE

class HgiVkInstance;
class HgiVkDevice;
class HgiVkSurface;
class HgiVkSwapchain;
struct HgiVkSurfaceDesc;


typedef std::vector<HgiVkDevice*> HgiVkDeviceVector;
typedef HgiVkSwapchain* HgiVkSwapchainHandle;
typedef HgiVkSurface* HgiVkSurfaceHandle;


/// \class HgiVk
///
/// Vulkan implementation of the Hydra Graphics Interface.
///
class HgiVk final : public Hgi
{
public:
    HGIVK_API
    HgiVk();

    HGIVK_API
    ~HgiVk();

    HGIVK_API
    void EndFrame() override;

    HGIVK_API
    HgiGraphicsEncoderUniquePtr CreateGraphicsEncoder(
        HgiGraphicsEncoderDesc const& desc) override;

    HGIVK_API
    HgiParallelGraphicsEncoderUniquePtr CreateParallelGraphicsEncoder(
        HgiGraphicsEncoderDesc const& desc,
        HgiPipelineHandle pipeline,
        const char* debugName=nullptr) override;

    HGIVK_API
    HgiBlitEncoderUniquePtr CreateBlitEncoder() override;

    HGIVK_API
    HgiTextureHandle CreateTexture(HgiTextureDesc const& desc) override;

    HGIVK_API
    void DestroyTexture(HgiTextureHandle* texHandle) override;

    HGIVK_API
    HgiBufferHandle CreateBuffer(HgiBufferDesc const& desc) override;

    HGIVK_API
    void DestroyBuffer(HgiBufferHandle* bufferHandle) override;

    HGIVK_API
    HgiPipelineHandle CreatePipeline(
        HgiPipelineDesc const& pipeDesc) override;

    HGIVK_API
    void DestroyPipeline(HgiPipelineHandle* pipeHandle) override;

    HGIVK_API
    HgiResourceBindingsHandle CreateResourceBindings(
        HgiResourceBindingsDesc const& desc) override;

    HGIVK_API
    void DestroyResourceBindings(
        HgiResourceBindingsHandle* resHandle) override;

    HGIVK_API
    HgiShaderFunctionHandle CreateShaderFunction(
        HgiShaderFunctionDesc const& desc) override;

    HGIVK_API
    void DestroyShaderFunction(
        HgiShaderFunctionHandle* shaderFunctionHandle) override;

    HGIVK_API
    HgiShaderProgramHandle CreateShaderProgram(
        HgiShaderProgramDesc const& desc) override;

    HGIVK_API
    void DestroyShaderProgram(
        HgiShaderProgramHandle* shaderProgramHandle) override;

    HGIVK_API
    void GetMemoryInfo(size_t* used, size_t* unused) override;

    HGIVK_API
    HgiTimeQueryVector const& GetTimeQueries() override;

public:

    // Returns the Hgi vulkan instance.
    HGIVK_API
    HgiVkInstance* GetVulkanInstance() const;

    // Returns the vkInstace as void pointer.
    // This makes it a little easier to pass the vkInstance around without
    // having to deal with vulkan headers everywhere that includes hgi.
    HGIVK_API
    void* GetVkInstance();

    // Returns the primary device. This device must support presentation and
    // resource creation.
    HGIVK_API
    HgiVkDevice* GetPrimaryDevice() const;

    /// Create a new surface
    HGIVK_API
    HgiVkSurfaceHandle CreateSurface(HgiVkSurfaceDesc const& desc);

    /// Destroy a surface
    HGIVK_API
    void DestroySurface(HgiVkSurfaceHandle* surfaceHandle);

    /// Create a new swapcain
    HGIVK_API
    HgiVkSwapchainHandle CreateSwapchain(HgiVkSurfaceHandle surfaceHandle);

    /// Destroy a swapcain
    HGIVK_API
    void DestroySwapchain(HgiVkSwapchainHandle* swapchainHandle);

    /// Destroys all devices and vulkan instance.
    /// Should be called once during application shutdown.
    HGIVK_API
    void DestroyHgiVk();

    /// Returns the max number of threads we expect to run.
    HGIVK_API
    static uint32_t GetThreadCount();

private:
    HgiVk & operator=(const HgiVk&) = delete;
    HgiVk(const HgiVk&) = delete;

    // Begin a new frame of rendering.
    // This call is managed internally. See EndFrame definition for details.
    void _BeginFrame();

private:
    HgiVkInstance* _instance;
    HgiVkDeviceVector _devices;
    bool _frameStarted;
};


// Global / static access to Vulkan Hydra graphics interface.
// Multiple plugins want to use the same HgiVkDevice, so that resources are
// shared (e.g. UI wants to display images produced by renderer)
#ifdef __cplusplus
extern "C" {
#endif
    HGIVK_API
    HgiVk* HgiVkGetHydraGraphicsInterface();
#ifdef __cplusplus
}
#endif


PXR_NAMESPACE_CLOSE_SCOPE

#endif
