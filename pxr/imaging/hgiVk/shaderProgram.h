#ifndef PXR_IMAGING_HGIVK_SHADERPROGRAM_H
#define PXR_IMAGING_HGIVK_SHADERPROGRAM_H

#include <vector>

#include "pxr/imaging/hgi/shaderProgram.h"

#include "pxr/imaging/hgiVk/api.h"
#include "pxr/imaging/hgiVk/shaderFunction.h"

PXR_NAMESPACE_OPEN_SCOPE


///
/// \class HgiVkShaderProgram
///
/// Vulkan implementation of HgiShaderProgram
///
class HgiVkShaderProgram final : public HgiShaderProgram {
public:
    HGIVK_API
    HgiVkShaderProgram(HgiShaderProgramDesc const& desc);

    HGIVK_API
    virtual ~HgiVkShaderProgram();

    HGIVK_API
    HgiShaderFunctionHandleVector const& GetShaderFunctions() const;

private:
    HgiVkShaderProgram() = delete;
    HgiVkShaderProgram & operator=(const HgiVkShaderProgram&) = delete;
    HgiVkShaderProgram(const HgiVkShaderProgram&) = delete;

private:
    HgiShaderProgramDesc _descriptor;
};


PXR_NAMESPACE_CLOSE_SCOPE

#endif