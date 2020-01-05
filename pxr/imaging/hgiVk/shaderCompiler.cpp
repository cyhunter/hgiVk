#if defined(_WIN32)
    #include <Windows.h>
#endif

#include <string>
#include <vector>

#include "pxr/base/tf/diagnostic.h"

#include "pxr/imaging/hgiVk/shaderCompiler.h"
#include "pxr/imaging/hgiVk/glslang/glslang/Public/ShaderLang.h"
#include "pxr/imaging/hgiVk/glslang/SPIRV/GlslangToSpv.h"
#include "pxr/imaging/hgiVk/glslang/OGLCompilersDLL/InitializeDll.h"

PXR_NAMESPACE_OPEN_SCOPE

static bool _glslangInitialized = false;

EShLanguage
_GetShaderStage(HgiShaderStage stage)
{
    switch(stage) {
        default: TF_CODING_ERROR("Unknown stage"); return EShLangCount;

        case HgiShaderStageVertex: return EShLangVertex;
        // todo missing in Hgi
        //case HgiShaderStageTessControl: return EShLangTessControl;
        //case HgiShaderStageTessEval: return EShLangTessEvaluation;
        //case HgiShaderStageGeometry: return EShLangGeometry;
        case HgiShaderStageFragment: return EShLangFragment;
        case HgiShaderStageCompute: return EShLangCompute;
    }
}

HgiVkShaderCompiler::HgiVkShaderCompiler()
{
    // Initialize() should be called exactly once per PROCESS.
    if (!_glslangInitialized) {
        glslang::InitializeProcess();
        _glslangInitialized = true;
    }
}

HgiVkShaderCompiler::~HgiVkShaderCompiler()
{
    if (_glslangInitialized) {
        glslang::FinalizeProcess();
        _glslangInitialized = false;
    }
}

void
HgiVkShaderCompiler::AddIncludeDir(const char* dir)
{
    _dirStackIncluder.pushExternalLocalDirectory(dir);
}

bool
HgiVkShaderCompiler::CompileGLSL(
    const char* name,
    const char* shaderCodes,
    uint8_t numShaderCodes,
    HgiShaderStage stage,
    std::vector<unsigned int>* spirvOUT,
    std::string* errors)
{
    // Hydra is multi-threaded so each new thread must init once.
    glslang::InitThread();

    if (numShaderCodes==0 || !spirvOUT) {
        if (errors) {
            errors->append("No shader to compile %s", name);
        }
        return false;
    }

    EShLanguage shaderType = _GetShaderStage(stage);
    glslang::TShader shader(shaderType);
    shader.setStrings(&shaderCodes, numShaderCodes);

    //
    // Set up Vulkan/SpirV Environment
    //

    // Maps approx to #define VULKAN 100
    int ClientInputSemanticsVersion = 100;

    glslang::EShTargetClientVersion vulkanClientVersion =
        glslang::EShTargetVulkan_1_0;

    glslang::EShTargetLanguageVersion targetVersion =
        glslang::EShTargetSpv_1_0;

    shader.setEnvInput(
        glslang::EShSourceGlsl,
        shaderType,
        glslang::EShClientVulkan,
        ClientInputSemanticsVersion);

    shader.setEnvClient(glslang::EShClientVulkan, vulkanClientVersion);
    shader.setEnvTarget(glslang::EShTargetSpv, targetVersion);

    //
    // Setup compiler limits/caps
    //

    // Reference see file: StandAlone/ResourceLimits.cpp
    // https://github.com/KhronosGroup/glslang
    //
    // https://github.com/KhronosGroup/glslang/blob/master/glslang/
    // OSDependent/Web/glslang.js.cpp
    const TBuiltInResource DefaultTBuiltInResource = {
        /* .MaxLights = */ 32,
        /* .MaxClipPlanes = */ 6,
        /* .MaxTextureUnits = */ 32,
        /* .MaxTextureCoords = */ 32,
        /* .MaxVertexAttribs = */ 64,
        /* .MaxVertexUniformComponents = */ 4096,
        /* .MaxVaryingFloats = */ 64,
        /* .MaxVertexTextureImageUnits = */ 32,
        /* .MaxCombinedTextureImageUnits = */ 80,
        /* .MaxTextureImageUnits = */ 32,
        /* .MaxFragmentUniformComponents = */ 4096,
        /* .MaxDrawBuffers = */ 32,
        /* .MaxVertexUniformVectors = */ 128,
        /* .MaxVaryingVectors = */ 8,
        /* .MaxFragmentUniformVectors = */ 16,
        /* .MaxVertexOutputVectors = */ 16,
        /* .MaxFragmentInputVectors = */ 15,
        /* .MinProgramTexelOffset = */ -8,
        /* .MaxProgramTexelOffset = */ 7,
        /* .MaxClipDistances = */ 8,
        /* .MaxComputeWorkGroupCountX = */ 65535,
        /* .MaxComputeWorkGroupCountY = */ 65535,
        /* .MaxComputeWorkGroupCountZ = */ 65535,
        /* .MaxComputeWorkGroupSizeX = */ 1024,
        /* .MaxComputeWorkGroupSizeY = */ 1024,
        /* .MaxComputeWorkGroupSizeZ = */ 64,
        /* .MaxComputeUniformComponents = */ 1024,
        /* .MaxComputeTextureImageUnits = */ 16,
        /* .MaxComputeImageUniforms = */ 8,
        /* .MaxComputeAtomicCounters = */ 8,
        /* .MaxComputeAtomicCounterBuffers = */ 1,
        /* .MaxVaryingComponents = */ 60,
        /* .MaxVertexOutputComponents = */ 64,
        /* .MaxGeometryInputComponents = */ 64,
        /* .MaxGeometryOutputComponents = */ 128,
        /* .MaxFragmentInputComponents = */ 128,
        /* .MaxImageUnits = */ 8,
        /* .MaxCombinedImageUnitsAndFragmentOutputs = */ 8,
        /* .MaxCombinedShaderOutputResources = */ 8,
        /* .MaxImageSamples = */ 0,
        /* .MaxVertexImageUniforms = */ 0,
        /* .MaxTessControlImageUniforms = */ 0,
        /* .MaxTessEvaluationImageUniforms = */ 0,
        /* .MaxGeometryImageUniforms = */ 0,
        /* .MaxFragmentImageUniforms = */ 8,
        /* .MaxCombinedImageUniforms = */ 8,
        /* .MaxGeometryTextureImageUnits = */ 16,
        /* .MaxGeometryOutputVertices = */ 256,
        /* .MaxGeometryTotalOutputComponents = */ 1024,
        /* .MaxGeometryUniformComponents = */ 1024,
        /* .MaxGeometryVaryingComponents = */ 64,
        /* .MaxTessControlInputComponents = */ 128,
        /* .MaxTessControlOutputComponents = */ 128,
        /* .MaxTessControlTextureImageUnits = */ 16,
        /* .MaxTessControlUniformComponents = */ 1024,
        /* .MaxTessControlTotalOutputComponents = */ 4096,
        /* .MaxTessEvaluationInputComponents = */ 128,
        /* .MaxTessEvaluationOutputComponents = */ 128,
        /* .MaxTessEvaluationTextureImageUnits = */ 16,
        /* .MaxTessEvaluationUniformComponents = */ 1024,
        /* .MaxTessPatchComponents = */ 120,
        /* .MaxPatchVertices = */ 32,
        /* .MaxTessGenLevel = */ 64,
        /* .MaxViewports = */ 16,
        /* .MaxVertexAtomicCounters = */ 0,
        /* .MaxTessControlAtomicCounters = */ 0,
        /* .MaxTessEvaluationAtomicCounters = */ 0,
        /* .MaxGeometryAtomicCounters = */ 0,
        /* .MaxFragmentAtomicCounters = */ 8,
        /* .MaxCombinedAtomicCounters = */ 8,
        /* .MaxAtomicCounterBindings = */ 1,
        /* .MaxVertexAtomicCounterBuffers = */ 0,
        /* .MaxTessControlAtomicCounterBuffers = */ 0,
        /* .MaxTessEvaluationAtomicCounterBuffers = */ 0,
        /* .MaxGeometryAtomicCounterBuffers = */ 0,
        /* .MaxFragmentAtomicCounterBuffers = */ 1,
        /* .MaxCombinedAtomicCounterBuffers = */ 1,
        /* .MaxAtomicCounterBufferSize = */ 16384,
        /* .MaxTransformFeedbackBuffers = */ 4,
        /* .MaxTransformFeedbackInterleavedComponents = */ 64,
        /* .MaxCullDistances = */ 8,
        /* .MaxCombinedClipAndCullDistances = */ 8,
        /* .MaxSamples = */ 4,
        /* .maxMeshOutputVerticesNV = */ 256,
        /* .maxMeshOutputPrimitivesNV = */ 512,
        /* .maxMeshWorkGroupSizeX_NV = */ 32,
        /* .maxMeshWorkGroupSizeY_NV = */ 1,
        /* .maxMeshWorkGroupSizeZ_NV = */ 1,
        /* .maxTaskWorkGroupSizeX_NV = */ 32,
        /* .maxTaskWorkGroupSizeY_NV = */ 1,
        /* .maxTaskWorkGroupSizeZ_NV = */ 1,
        /* .maxMeshViewCountNV = */ 4,

        /* .limits = */ {
            /* .nonInductiveForLoops = */ 1,
            /* .whileLoops = */ 1,
            /* .doWhileLoops = */ 1,
            /* .generalUniformIndexing = */ 1,
            /* .generalAttributeMatrixVectorIndexing = */ 1,
            /* .generalVaryingIndexing = */ 1,
            /* .generalSamplerIndexing = */ 1,
            /* .generalVariableIndexing = */ 1,
            /* .generalConstantMatrixVectorIndexing = */ 1,
        }};

    EShMessages messages = (EShMessages) (EShMsgSpvRules | EShMsgVulkanRules);

    //
    // Run PreProcess
    //
    const int defaultVersion = 100;
    std::string preprocessedGLSL;

    bool preProcessOK = shader.preprocess(
            &DefaultTBuiltInResource,
            defaultVersion,
            ECoreProfile,
            false,
            false,
            messages,
            &preprocessedGLSL,
            _dirStackIncluder); // Alt: glslang::TShader::ForbidIncluder

    if (!preProcessOK) {
        if (errors) {
            errors->append("GLSL Preprocessing Failed for: ");
            errors->append(name);
            errors->append("\n");
            errors->append(shader.getInfoLog());
            errors->append(shader.getInfoDebugLog());
        }
        return false;
    }

    //
    // Parse and link shader
    //
    const char* preprocessedCStr = preprocessedGLSL.c_str();
    shader.setStrings(&preprocessedCStr, 1);

    if (!shader.parse(&DefaultTBuiltInResource, 100, false, messages)) {
        if (errors) {
            errors->append("GLSL Parsing Failed for: ");
            errors->append(name);
            errors->append("\n");
            errors->append(shader.getInfoLog());
            errors->append(shader.getInfoDebugLog());
        }
        return false;
    }

    glslang::TProgram program;
    program.addShader(&shader);

    if(!program.link(messages)) {
        if (errors) {
            errors->append("GLSL linking failed for: ");
            errors->append(name);
            errors->append("\n");
            errors->append(shader.getInfoLog());
            errors->append(shader.getInfoDebugLog());
        }
        return false;
    }

    //
    // Convert to SPIRV
    //
    spv::SpvBuildLogger logger;
    glslang::SpvOptions spvOptions;
    spvOptions.generateDebugInfo = false;
    spvOptions.optimizeSize = false;
    spvOptions.disassemble = false;
    spvOptions.validate = false;

    glslang::GlslangToSpv(
        *program.getIntermediate(shaderType),
        *spirvOUT,
        &logger,
        &spvOptions);

    if (logger.getAllMessages().length() > 0) {
        if (errors) {
            errors->append(logger.getAllMessages().c_str());
        }
    }

    // XXX glslang can output the spirv binary for us:
    // glslang::OutputSpvBin(*spirvOUT, "filename.spv");

    return true;
}

PXR_NAMESPACE_CLOSE_SCOPE
