#include "pxr/imaging/hgi/resourceBindings.h"

PXR_NAMESPACE_OPEN_SCOPE

HgiResourceBindings::HgiResourceBindings(HgiResourceBindingsDesc const& desc)
{
}

HgiResourceBindings::~HgiResourceBindings()
{
}

HgiVertexAttributeDesc::HgiVertexAttributeDesc()
    : format(HgiFormatFloat32Vec4)
    , offset(0)
    , shaderBindLocation(0)
{
}

bool operator==(
    const HgiVertexAttributeDesc& lhs,
    const HgiVertexAttributeDesc& rhs)
{
    return lhs.format == rhs.format &&
           lhs.offset == rhs.offset &&
           lhs.shaderBindLocation == rhs.shaderBindLocation;
}

bool operator!=(
    const HgiVertexAttributeDesc& lhs,
    const HgiVertexAttributeDesc& rhs)
{
    return !(lhs == rhs);
}

HgiVertexBufferDesc::HgiVertexBufferDesc()
    : bindingIndex(0)
    , vertexStride(0)
{
}

bool operator==(
    const HgiVertexBufferDesc& lhs,
    const HgiVertexBufferDesc& rhs)
{
    return lhs.bindingIndex == rhs.bindingIndex &&
           lhs.vertexAttributes == rhs.vertexAttributes &&
           lhs.vertexStride == rhs.vertexStride;
}

bool operator!=(
    const HgiVertexBufferDesc& lhs,
    const HgiVertexBufferDesc& rhs)
{
    return !(lhs == rhs);
}

HgiBufferBindDesc::HgiBufferBindDesc()
    : bindingIndex(0)
    , stageUsage(HgiShaderStageVertex)
{
}

bool operator==(
    const HgiBufferBindDesc& lhs,
    const HgiBufferBindDesc& rhs)
{
    return lhs.buffers == rhs.buffers &&
           lhs.resourceType == rhs.resourceType &&
           lhs.offsets == rhs.offsets &&
           lhs.bindingIndex == rhs.bindingIndex &&
           lhs.stageUsage == rhs.stageUsage;
}

bool operator!=(
    const HgiBufferBindDesc& lhs,
    const HgiBufferBindDesc& rhs)
{
    return !(lhs == rhs);
}

HgiTextureBindDesc::HgiTextureBindDesc()
    : bindingIndex(0)
    , stageUsage(HgiShaderStageFragment)
{
}

bool operator==(
    const HgiTextureBindDesc& lhs,
    const HgiTextureBindDesc& rhs)
{
    return lhs.textures == rhs.textures &&
           lhs.resourceType == rhs.resourceType &&
           lhs.bindingIndex == rhs.bindingIndex &&
           lhs.stageUsage == rhs.stageUsage;
}

bool operator!=(
    const HgiTextureBindDesc& lhs,
    const HgiTextureBindDesc& rhs)
{
    return !(lhs == rhs);
}

HgiPushConstantDesc::HgiPushConstantDesc()
    : offset(0)
    , byteSize(0)
    , stageUsage(HgiShaderStageFragment)
{
}

bool operator==(
    const HgiPushConstantDesc& lhs,
    const HgiPushConstantDesc& rhs)
{
    return lhs.offset == rhs.offset &&
           lhs.byteSize == rhs.byteSize &&
           lhs.stageUsage == rhs.stageUsage;
}

bool operator!=(
    const HgiPushConstantDesc& lhs,
    const HgiPushConstantDesc& rhs)
{
    return !(lhs == rhs);
}

HgiResourceBindingsDesc::HgiResourceBindingsDesc()
    : pipelineType(HgiPipelineTypeGraphics)
{
}

bool operator==(
    const HgiResourceBindingsDesc& lhs,
    const HgiResourceBindingsDesc& rhs)
{
    return lhs.pipelineType == rhs.pipelineType &&
           lhs.buffers == rhs.buffers &&
           lhs.textures == rhs.textures &&
           lhs.pushConstants == rhs.pushConstants &&
           lhs.vertexBuffers == rhs.vertexBuffers;
}

bool operator!=(
    const HgiResourceBindingsDesc& lhs,
    const HgiResourceBindingsDesc& rhs)
{
    return !(lhs == rhs);
}

PXR_NAMESPACE_CLOSE_SCOPE
