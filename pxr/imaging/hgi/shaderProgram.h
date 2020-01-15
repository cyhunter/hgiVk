#ifndef PXR_IMAGING_HGI_SHADERPROGRAM_H
#define PXR_IMAGING_HGI_SHADERPROGRAM_H

#include <vector>

#include "pxr/pxr.h"
#include "pxr/imaging/hgi/api.h"
#include "pxr/imaging/hgi/enums.h"
#include "pxr/imaging/hgi/shaderFunction.h"
#include "pxr/imaging/hgi/types.h"


PXR_NAMESPACE_OPEN_SCOPE

struct HgiShaderProgramDesc;


///
/// \class HgiShaderProgram
///
/// Represents a collection of shader functions.
///
class HgiShaderProgram {
public:
    HGI_API
    HgiShaderProgram(HgiShaderProgramDesc const& desc);

    HGI_API
    virtual ~HgiShaderProgram();

private:
    HgiShaderProgram() = delete;
    HgiShaderProgram & operator=(const HgiShaderProgram&) = delete;
    HgiShaderProgram(const HgiShaderProgram&) = delete;
};

typedef HgiShaderProgram* HgiShaderProgramHandle;
typedef std::vector<HgiShaderProgramHandle> HgiShaderProgramHandleVector;


/// \struct HgiShaderProgramDesc
///
/// Describes the properties needed to create a GPU shader program.
///
/// <ul>
/// <li>shaderFunctions:
///   Holds handles to shader functions for each shader stage.</li>
/// </ul>
///
struct HgiShaderProgramDesc {
    HGI_API
    HgiShaderProgramDesc();

    std::string debugName;
    HgiShaderFunctionHandleVector shaderFunctions;
};

HGI_API
inline bool operator==(
    const HgiShaderProgramDesc& lhs,
    const HgiShaderProgramDesc& rhs);

HGI_API
inline bool operator!=(
    const HgiShaderProgramDesc& lhs,
    const HgiShaderProgramDesc& rhs);


PXR_NAMESPACE_CLOSE_SCOPE

#endif