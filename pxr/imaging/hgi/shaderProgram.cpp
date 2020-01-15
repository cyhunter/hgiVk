#include "pxr/imaging/hgi/shaderProgram.h"

PXR_NAMESPACE_OPEN_SCOPE

HgiShaderProgram::HgiShaderProgram(HgiShaderProgramDesc const& desc)
{
}

HgiShaderProgram::~HgiShaderProgram()
{
}

HgiShaderProgramDesc::HgiShaderProgramDesc()
    : shaderFunctions(HgiShaderFunctionHandleVector())
{
}

bool operator==(
    const HgiShaderProgramDesc& lhs,
    const HgiShaderProgramDesc& rhs)
{
    return lhs.debugName == rhs.debugName &&
           lhs.shaderFunctions == rhs.shaderFunctions;
}

bool operator!=(
    const HgiShaderProgramDesc& lhs,
    const HgiShaderProgramDesc& rhs)
{
    return !(lhs == rhs);
}

PXR_NAMESPACE_CLOSE_SCOPE
