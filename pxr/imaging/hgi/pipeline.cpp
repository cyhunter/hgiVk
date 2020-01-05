#include "pxr/imaging/hgi/pipeline.h"

PXR_NAMESPACE_OPEN_SCOPE

HgiPipeline::HgiPipeline(HgiPipelineDesc const& desc)
{
}

HgiPipeline::~HgiPipeline()
{
}

HgiMultiSampleState::HgiMultiSampleState()
    : rasterizationSamples(HgiSampleCount1)
    , alphaToCoverageEnable(false)
    , sampleShadingEnable(false)
    , samplesPerFragment(0.5f)
{
}

bool operator==(
    const HgiMultiSampleState& lhs,
    const HgiMultiSampleState& rhs)
{
    return lhs.rasterizationSamples == rhs.rasterizationSamples &&
           lhs.alphaToCoverageEnable == rhs.alphaToCoverageEnable &&
           lhs.sampleShadingEnable == rhs.sampleShadingEnable &&
           lhs.samplesPerFragment == rhs.samplesPerFragment;
}

bool operator!=(
    const HgiMultiSampleState& lhs,
    const HgiMultiSampleState& rhs)
{
    return !(lhs == rhs);
}

HgiRasterizationState::HgiRasterizationState()
    : polygonMode(HgiPolygonModeFill)
    , lineWidth(1.0f)
    , cullMode(HgiCullModeBack)
    , winding(HgiWindingCounterClockwise)
{
}

bool operator==(
    const HgiRasterizationState& lhs,
    const HgiRasterizationState& rhs)
{
    return lhs.polygonMode == rhs.polygonMode &&
           lhs.lineWidth == rhs.lineWidth &&
           lhs.cullMode == rhs.cullMode &&
           lhs.winding == rhs.winding;
}

bool operator!=(
    const HgiRasterizationState& lhs,
    const HgiRasterizationState& rhs)
{
    return !(lhs == rhs);
}

HgiPipelineDesc::HgiPipelineDesc()
    : pipelineType(HgiPipelineTypeGraphics)
    , shaderProgram(nullptr)
    , depthState(HgiDepthStateBitsDepthNone)
    , depthCompareOp(HgiCompareOpLessOrEqual)
{
}

bool operator==(
    const HgiPipelineDesc& lhs,
    const HgiPipelineDesc& rhs)
{
    return lhs.pipelineType == rhs.pipelineType &&
           lhs.resourceBindings == rhs.resourceBindings &&
           lhs.shaderProgram == rhs.shaderProgram &&
           lhs.depthState == rhs.depthState &&
           lhs.depthCompareOp == rhs.depthCompareOp &&
           lhs.multiSampleState == rhs.multiSampleState &&
           lhs.rasterizationState == rhs.rasterizationState;
}

bool operator!=(
    const HgiPipelineDesc& lhs,
    const HgiPipelineDesc& rhs)
{
    return !(lhs == rhs);
}

PXR_NAMESPACE_CLOSE_SCOPE
