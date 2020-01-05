#ifndef PXR_IMAGING_HGIVK_SHADERFUNCTION_H
#define PXR_IMAGING_HGIVK_SHADERFUNCTION_H

#include "pxr/imaging/hgi/shaderFunction.h"

#include "pxr/imaging/hgiVk/api.h"
#include "pxr/imaging/hgiVk/vulkan.h"

PXR_NAMESPACE_OPEN_SCOPE

class HgiVkDevice;


///
/// \class HgiVkShaderFunction
///
/// Vulkan implementation of HgiShaderFunction
///
class HgiVkShaderFunction final : public HgiShaderFunction {
public:
    HGIVK_API
    HgiVkShaderFunction(
        HgiVkDevice* device,
        HgiShaderFunctionDesc const& desc);

    HGIVK_API
    virtual ~HgiVkShaderFunction();

    /// Returns false if any shader compile errors occured.
    HGIVK_API
    bool IsValid() const override;

    /// Returns shader compile errors
    HGIVK_API
    std::string const& GetCompileErrors() override;

    /// Returns the shader stage this function operates in.
    HGIVK_API
    VkShaderStageFlagBits GetShaderStage() const;

    /// Returns the binary shader module of the shader function.
    HGIVK_API
    VkShaderModule GetShaderModule() const;

    /// Returns the shader entry function name (usually "main").
    HGIVK_API
    const char* GetShaderFunctionName() const;

private:
    HgiVkShaderFunction() = delete;
    HgiVkShaderFunction & operator=(const HgiVkShaderFunction&) = delete;
    HgiVkShaderFunction(const HgiVkShaderFunction&) = delete;

private:
    HgiVkDevice* _device;
    HgiShaderFunctionDesc _descriptor;
    std::string _errors;

    VkShaderModule _vkShaderModule;
};


PXR_NAMESPACE_CLOSE_SCOPE

#endif
