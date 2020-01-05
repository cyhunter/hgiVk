#include "pxr/imaging/hgi/shaderFunction.h"

PXR_NAMESPACE_OPEN_SCOPE

HgiShaderFunction::HgiShaderFunction(HgiShaderFunctionDesc const& desc)
{
}

HgiShaderFunction::~HgiShaderFunction()
{
}

HgiShaderFunctionDesc::HgiShaderFunctionDesc()
    : shaderStage(HgiShaderStageVertex)
    , shaderCode(std::string())
{
}

bool operator==(
    const HgiShaderFunctionDesc& lhs,
    const HgiShaderFunctionDesc& rhs)
{
    return lhs.shaderStage == rhs.shaderStage &&
           lhs.shaderCode == rhs.shaderCode;
}

bool operator!=(
    const HgiShaderFunctionDesc& lhs,
    const HgiShaderFunctionDesc& rhs)
{
    return !(lhs == rhs);
}

PXR_NAMESPACE_CLOSE_SCOPE
