#ifndef PXR_IMAGING_HGI_SHADERFUNCTION_H
#define PXR_IMAGING_HGI_SHADERFUNCTION_H

#include <string>
#include <vector>

#include "pxr/pxr.h"
#include "pxr/imaging/hgi/api.h"
#include "pxr/imaging/hgi/enums.h"
#include "pxr/imaging/hgi/types.h"


PXR_NAMESPACE_OPEN_SCOPE

struct HgiShaderFunctionDesc;


///
/// \class HgiShaderFunction
///
/// Represents one shader stage function (code snippet).
///
class HgiShaderFunction {
public:
    HGI_API
    HgiShaderFunction(HgiShaderFunctionDesc const& desc);

    HGI_API
    virtual ~HgiShaderFunction();

    /// Returns false if any shader compile errors occured.
    HGI_API
    virtual bool IsValid() const = 0;

    /// Returns shader compile errors.
    HGI_API
    virtual std::string const& GetCompileErrors() = 0;

private:
    HgiShaderFunction() = delete;
    HgiShaderFunction & operator=(const HgiShaderFunction&) = delete;
    HgiShaderFunction(const HgiShaderFunction&) = delete;
};

typedef HgiShaderFunction* HgiShaderFunctionHandle;
typedef std::vector<HgiShaderFunctionHandle> HgiShaderFunctionHandleVector;


/// \struct HgiShaderFunctionDesc
///
/// Describes the properties needed to create a GPU shader function.
///
/// <ul>
/// <li>shaderCode:
///   The ascii shader code.</li>
/// </ul>
///
struct HgiShaderFunctionDesc {
    HGI_API
    HgiShaderFunctionDesc();

    std::string debugName;
    HgiShaderStage shaderStage;
    std::string shaderCode;
};

HGI_API
inline bool operator==(
    const HgiShaderFunctionDesc& lhs,
    const HgiShaderFunctionDesc& rhs);

HGI_API
inline bool operator!=(
    const HgiShaderFunctionDesc& lhs,
    const HgiShaderFunctionDesc& rhs);


PXR_NAMESPACE_CLOSE_SCOPE

#endif
