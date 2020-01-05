#ifndef PXR_IMAGING_HGIVK_SHADERCOMPILER_H
#define PXR_IMAGING_HGIVK_SHADERCOMPILER_H

#include <stdint.h>
#include <stdlib.h>

#include <vector>

#include "pxr/pxr.h"
#include "pxr/imaging/hgi/enums.h"
#include "pxr/imaging/hgiVk/api.h"
#include "pxr/imaging/hgiVk/dirStackFileIncluder.h"


PXR_NAMESPACE_OPEN_SCOPE


///
/// \class HgiVkShaderCompiler
///
/// Wrapper for glslang - a glsl compiler, validator and SPIRV backend.
///
class HgiVkShaderCompiler final {
public:
    HGIVK_API
    HgiVkShaderCompiler();

    HGIVK_API
    virtual ~HgiVkShaderCompiler();

    /// Adds an 'include' dir so #include statements can be resolved.
    HGIVK_API
    void AddIncludeDir(const char* dir);

    /// Compiles ascii shader code (glsl) into spirv binary code (spirvOut).
    /// Returns true if successful. Errors can optionally be captured.
    /// numShaderCodes determines how many strings are provided via shaderCodes.
    /// For #include statements to be resolved, AddIncludeDir() must be called
    /// before compiling shaders.
    /// 'name' is purely for debugging compile errors. It can be anything.
    HGIVK_API
    bool CompileGLSL(
        const char* name,
        const char* shaderCodes,
        uint8_t numShaderCodes,
        HgiShaderStage stage,
        std::vector<unsigned int>* spirvOUT,
        std::string* errors = nullptr);


private:
    HgiVkShaderCompiler & operator=(const HgiVkShaderCompiler&) = delete;
    HgiVkShaderCompiler(const HgiVkShaderCompiler&) = delete;

private:
    DirStackFileIncluder _dirStackIncluder;
};


PXR_NAMESPACE_CLOSE_SCOPE

#endif
