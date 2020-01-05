#ifndef PXR_IMAGING_HGI_PIPELINE_H
#define PXR_IMAGING_HGI_PIPELINE_H

#include <vector>

#include "pxr/pxr.h"
#include "pxr/imaging/hgi/api.h"
#include "pxr/imaging/hgi/enums.h"
#include "pxr/imaging/hgi/resourceBindings.h"
#include "pxr/imaging/hgi/shaderProgram.h"
#include "pxr/imaging/hgi/types.h"


PXR_NAMESPACE_OPEN_SCOPE

struct HgiPipelineDesc;
struct HgiMultiSampleState;


///
/// \class HgiPipeline
///
/// Represents a graphics platform independent GPU pipeline resource.
///
/// Base class for Hgi pipelines.
/// To the client (HdSt) pipeline resources are referred to via
/// opaque, stateless handles (HgiPipelineHandle).
///
class HgiPipeline {
public:
    HGI_API
    HgiPipeline(HgiPipelineDesc const& desc);

    HGI_API
    virtual ~HgiPipeline();

private:
    HgiPipeline() = delete;
    HgiPipeline & operator=(const HgiPipeline&) = delete;
    HgiPipeline(const HgiPipeline&) = delete;
};

typedef HgiPipeline* HgiPipelineHandle;
typedef std::vector<HgiPipelineHandle> HgiPipelineHandleVector;


/// \struct HgiMultiSampleState
///
/// Properties to configure multi sampling.
///
/// <ul>
/// <li>rasterizationSamples:
///   Number of samples used during rasterization (MSAA).
///   Enabling this can improve edge anti-aliasting.</li>
/// <li>alphaToCoverageEnable:
///   Fragment’s color.a determines coverage (screen door transparency).</li>
/// <li>sampleShadingEnable:
///   Enables sample shading (extra samples per fragment).
///   Can improve shader aliasing within the interior of topology.</li>
/// <li>samplesPerFragment:
///   Multiplier (0-1) that controls how many extra samples to take for
///   a fragment. 1.0 gives the max number of samples. The max number of samples
///   is determined by the rasterizationSamples.</li>
/// </ul>
///
struct HgiMultiSampleState {
    HGI_API
    HgiMultiSampleState();

    HgiSampleCount rasterizationSamples;
    bool alphaToCoverageEnable;
    bool sampleShadingEnable;
    float samplesPerFragment;
};

HGI_API
bool operator==(
    const HgiMultiSampleState& lhs,
    const HgiMultiSampleState& rhs);

HGI_API
bool operator!=(
    const HgiMultiSampleState& lhs,
    const HgiMultiSampleState& rhs);


/// \struct HgiRasterizationState
///
/// Properties to configure multi sampling.
///
/// <ul>
/// <li>polygonMode:
///   Determines the rasterization draw mode of primitve (triangles).</li>
/// <li>lineWidth:
///   The width of lines when polygonMode is set to line drawing.</li>
/// <li>cullMode:
///   Determines the culling rules for primitives (triangles).</li>
/// <li>winding:
///   The rule that determines what makes a front-facing primitive.</li>
/// </ul>
///
struct HgiRasterizationState {
    HGI_API
    HgiRasterizationState();

    HgiPolygonMode polygonMode;
    float lineWidth;
    HgiCullMode cullMode;
    HgiWinding winding;
};

HGI_API
bool operator==(
    const HgiRasterizationState& lhs,
    const HgiRasterizationState& rhs);

HGI_API
bool operator!=(
    const HgiRasterizationState& lhs,
    const HgiRasterizationState& rhs);


/// \struct HgiPipelineDesc
///
/// Describes the properties needed to create a GPU pipeline.
///
/// <ul>
/// <li>pipelineType:
///   Bind point for pipeline (Graphics or Compute).</li>
/// <li>resourceBindings:
///   The resource bindings that will be bound when the pipeline is used.
///   Primarily used to query the vertex attributes.</li>
/// <li>shaderProgram:
///   Shader functions/stages used in this pipeline.</li>
/// <li>depthState:
///   Describes depth state for a pipeline.</li>
/// <li>depthCompareOp:
///   The compare operation to use when depth test is enabled.</li>
/// <li>multiSampleState:
///   Various settings to control multi-sampling.</li>
/// <li>rasterizationState:
///   Various settings to control rasterization.</li>
/// </ul>
///
struct HgiPipelineDesc {
    HGI_API
    HgiPipelineDesc();

    HgiPipelineType pipelineType;
    HgiResourceBindingsHandle resourceBindings;
    HgiShaderProgramHandle shaderProgram;
    HgiDepthState depthState;
    HgiCompareOp depthCompareOp;
    HgiMultiSampleState multiSampleState;
    HgiRasterizationState rasterizationState;
};

HGI_API
bool operator==(
    const HgiPipelineDesc& lhs,
    const HgiPipelineDesc& rhs);

HGI_API
bool operator!=(
    const HgiPipelineDesc& lhs,
    const HgiPipelineDesc& rhs);


PXR_NAMESPACE_CLOSE_SCOPE

#endif
