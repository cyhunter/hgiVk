#ifndef PXR_IMAGING_HGI_RESOURCEBINDINGS_H
#define PXR_IMAGING_HGI_RESOURCEBINDINGS_H

#include <vector>

#include "pxr/pxr.h"
#include "pxr/imaging/hgi/api.h"
#include "pxr/imaging/hgi/buffer.h"
#include "pxr/imaging/hgi/enums.h"
#include "pxr/imaging/hgi/texture.h"
#include "pxr/imaging/hgi/types.h"


PXR_NAMESPACE_OPEN_SCOPE

// XXX move into HgiTexture
typedef std::vector<HgiTextureHandle> HgiTextureHandleVector;

struct HgiResourceBindingsDesc;

///
/// \class HgiResourceBindings
///
/// Represents a collection of buffers, texture and vertex attributes that will
/// be used by an encoder (and pipeline).
///
class HgiResourceBindings {
public:
    HGI_API
    HgiResourceBindings(HgiResourceBindingsDesc const& desc);

    HGI_API
    virtual ~HgiResourceBindings();

private:
    HgiResourceBindings() = delete;
    HgiResourceBindings & operator=(const HgiResourceBindings&) = delete;
    HgiResourceBindings(const HgiResourceBindings&) = delete;
};

typedef HgiResourceBindings* HgiResourceBindingsHandle;
typedef std::vector<HgiResourceBindingsHandle> HgiResourceBindingsHandleVector;


/// \struct HgiVertexAttributeDesc
///
/// Describes one attribute of a vertex.
///
/// <ul>
/// <li>format:
///   Format of the vertex attribute.</li>
/// <li>offset:
///    The byte offset of the attribute in vertex buffer</li>
/// <li>shaderBindLocation:
///    The location of thie attribute in the shader. layout(location = X)</li>
/// </ul>
///
struct HgiVertexAttributeDesc {
    HGI_API
    HgiVertexAttributeDesc();

    HgiFormat format;
    uint32_t offset;
    uint32_t shaderBindLocation;
};
typedef std::vector<HgiVertexAttributeDesc> HgiVertexAttributeDescVector;

HGI_API
bool operator==(
    const HgiVertexAttributeDesc& lhs,
    const HgiVertexAttributeDesc& rhs);

HGI_API
inline bool operator!=(
    const HgiVertexAttributeDesc& lhs,
    const HgiVertexAttributeDesc& rhs);


/// \struct HgiVertexBufferDesc
///
/// Describes the attributes of a vertex buffer.
///
/// <ul>
/// <li>bindingIndex:
///    Binding location for this vertex buffer.</li>
/// <li>vertexAttributes:
///   List of vertex attributes (in vertex buffer).</li>
/// <li>vertexStride:
///   The byte size of a vertex (distance between two vertices).</li>
/// </ul>
///
struct HgiVertexBufferDesc {
    HGI_API
    HgiVertexBufferDesc();

    uint32_t bindingIndex;
    HgiVertexAttributeDescVector vertexAttributes;
    uint32_t vertexStride;
};
typedef std::vector<HgiVertexBufferDesc> HgiVertexBufferDescVector;

HGI_API
bool operator==(
    const HgiVertexBufferDesc& lhs,
    const HgiVertexBufferDesc& rhs);

HGI_API
inline bool operator!=(
    const HgiVertexBufferDesc& lhs,
    const HgiVertexBufferDesc& rhs);


/// \struct HgiBufferBindDesc
///
/// Describes the binding information of one buffer.
///
/// <ul>
/// <li>buffers:
///   The buffer(s) to be bound.
///   If there are more than one buffer, the buffers will be put in an
///   array-of-buffers. Please note that different platforms have varying
///   limits to max buffers in an array.</li>
/// <li>resourceType:
///    The type of buffer(s) that is to be bound.
///    All buffers in the array must have the same type.
///    Note that vertex and index buffers are not bound to a resourceSet.
///    They are instead passed to the draw command.</li>
/// <li>offset:
///    Offset (in bytes) where data begins from the start of the buffer.
///    This if an offset for each buffer in 'buffers'.</li>
/// <li>bindingIndex:
///    Binding location for the buffer(s).</li>
/// <li>stageUsage:
///    What shader stage(s) the buffer will be used in.</li>
/// </ul>
///
struct HgiBufferBindDesc {
    HGI_API
    HgiBufferBindDesc();

    HgiBufferHandleVector buffers;
    std::vector<uint32_t> offsets;
    HgiBindResourceType resourceType;
    uint32_t bindingIndex;
    HgiShaderStage stageUsage;
};
typedef std::vector<HgiBufferBindDesc> HgiBufferBindDescVector;

HGI_API
bool operator==(
    const HgiBufferBindDesc& lhs,
    const HgiBufferBindDesc& rhs);

HGI_API
inline bool operator!=(
    const HgiBufferBindDesc& lhs,
    const HgiBufferBindDesc& rhs);


/// \struct HgiTextureBindDesc
///
/// Describes the binding information of one texture.
///
/// <ul>
/// <li>textures:
///   The texture(s) to be bound.
///   If there are more than one texture, the textures will be put in an
///   array-of-textures (not texture-array). Please note that different
///   platforms have varying limits to max textures in an array.</li>
/// <li>resourceType:
///    The type of the texture(s) that is to be bound.
///    All textures in the array must have the same type.</li>
/// <li>bindingIndex:
///    Binding location for the texture</li>
/// <li>stageUsage:
///    What shader stage(s) the buffer will be used in.</li>
/// </ul>
///
struct HgiTextureBindDesc {
    HGI_API
    HgiTextureBindDesc();

    HgiTextureHandleVector textures;
    HgiBindResourceType resourceType;
    uint32_t bindingIndex;
    HgiShaderStage stageUsage;
};
typedef std::vector<HgiTextureBindDesc> HgiTextureBindDescVector;

HGI_API
bool operator==(
    const HgiTextureBindDesc& lhs,
    const HgiTextureBindDesc& rhs);

HGI_API
bool operator!=(
    const HgiTextureBindDesc& lhs,
    const HgiTextureBindDesc& rhs);


/// \struct HgiPushConstantDesc
///
/// Push constants is a small amount, but very quick, uniform data for shaders.
///
/// <ul>
/// <li>offset:
///   Start of the push constants in bytes.</li>
/// <li>byteSize:
///    Size of the push constants. (max is usually small: 128 bytes)</li>
/// <li>stageUsage:
///    What shader stage(s) the push constants will be used in.</li>
/// </ul>
///
struct HgiPushConstantDesc {
    HGI_API
    HgiPushConstantDesc();

    uint32_t offset;
    uint32_t byteSize;
    HgiShaderStage stageUsage;
};
typedef std::vector<HgiPushConstantDesc> HgiPushConstantDescVector;

HGI_API
bool operator==(
    const HgiPushConstantDesc& lhs,
    const HgiPushConstantDesc& rhs);

HGI_API
bool operator!=(
    const HgiPushConstantDesc& lhs,
    const HgiPushConstantDesc& rhs);


/// \struct HgiResourceBindingsDesc
///
/// Describes a set of resources that are bound to the GPU during encoding.
///
/// <ul>
/// <li>pipelineType:
///   Bind point for pipeline.</li>
/// <li>buffers:
///   The buffers to be bound (E.g. uniform or shader storage).</li>
/// <li>textures:
///   The textures to be bound.</li>
/// <li>pushConstants:
///   Description of the Push / Function constants.
///   The actual push constant data is set via GraphicsEncoder.</li>
/// <li>vertexBuffers:
///   Description of the vertex buffers (per-vertex attributes).
///   The actual VBOs are bound via GraphicsEncoder.</li>
/// </ul>
///
struct HgiResourceBindingsDesc {
    HGI_API
    HgiResourceBindingsDesc();

    HgiPipelineType pipelineType;
    HgiBufferBindDescVector buffers;
    HgiTextureBindDescVector textures;
    HgiPushConstantDescVector pushConstants;
    HgiVertexBufferDescVector vertexBuffers;
};

HGI_API
bool operator==(
    const HgiResourceBindingsDesc& lhs,
    const HgiResourceBindingsDesc& rhs);

HGI_API
bool operator!=(
    const HgiResourceBindingsDesc& lhs,
    const HgiResourceBindingsDesc& rhs);


PXR_NAMESPACE_CLOSE_SCOPE

#endif