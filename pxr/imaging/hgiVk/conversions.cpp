#include "pxr/base/tf/diagnostic.h"
#include "pxr/imaging/hgi/enums.h"
#include "pxr/imaging/hgi/types.h"

#include "pxr/imaging/hgiVk/vulkan.h"
#include "pxr/imaging/hgiVk/conversions.h"

PXR_NAMESPACE_OPEN_SCOPE


static const uint32_t
_LoadOpTable[][2] =
{
    {HgiAttachmentLoadOpDontCare, VK_ATTACHMENT_LOAD_OP_DONT_CARE},
    {HgiAttachmentLoadOpClear,    VK_ATTACHMENT_LOAD_OP_CLEAR},
    {HgiAttachmentLoadOpLoad,     VK_ATTACHMENT_LOAD_OP_LOAD}
};

static const uint32_t
_StoreOpTable[][2] =
{
    {HgiAttachmentStoreOpDontCare, VK_ATTACHMENT_STORE_OP_DONT_CARE},
    {HgiAttachmentStoreOpStore,    VK_ATTACHMENT_STORE_OP_STORE}
};

static const uint32_t
_FormatTable[HgiFormatCount][3] =
{
    // HGI FORMAT              VK FORMAT                     NUM BYTES
    {HgiFormatUNorm8,      VK_FORMAT_R8_UNORM,            sizeof(uint8_t)*1 },
    {HgiFormatUNorm8Vec2,  VK_FORMAT_R8G8_UNORM,          sizeof(uint8_t)*2 },
    {HgiFormatUNorm8Vec3,  VK_FORMAT_R8G8B8_UNORM,        sizeof(uint8_t)*3 },
    {HgiFormatUNorm8Vec4,  VK_FORMAT_R8G8B8A8_UNORM,      sizeof(uint8_t)*4 },
    {HgiFormatSNorm8,      VK_FORMAT_R8_SNORM,            sizeof(int8_t)*1 },
    {HgiFormatSNorm8Vec2,  VK_FORMAT_R8G8_SNORM,          sizeof(int8_t)*2 },
    {HgiFormatSNorm8Vec3,  VK_FORMAT_R8G8B8_SNORM,        sizeof(int8_t)*3 },
    {HgiFormatSNorm8Vec4,  VK_FORMAT_R8G8B8A8_SNORM,      sizeof(int8_t)*4 },
    {HgiFormatFloat16,     VK_FORMAT_R16_SFLOAT,          sizeof(uint16_t)*1 },
    {HgiFormatFloat16Vec2, VK_FORMAT_R16G16_SFLOAT,       sizeof(uint16_t)*2 },
    {HgiFormatFloat16Vec3, VK_FORMAT_R16G16B16_SFLOAT,    sizeof(uint16_t)*3 },
    {HgiFormatFloat16Vec4, VK_FORMAT_R16G16B16A16_SFLOAT, sizeof(uint16_t)*4 },
    {HgiFormatFloat32,     VK_FORMAT_R32_SFLOAT,          sizeof(float)*1 },
    {HgiFormatFloat32Vec2, VK_FORMAT_R32G32_SFLOAT,       sizeof(float)*2 },
    {HgiFormatFloat32Vec3, VK_FORMAT_R32G32B32_SFLOAT,    sizeof(float)*3 },
    {HgiFormatFloat32Vec4, VK_FORMAT_R32G32B32A32_SFLOAT, sizeof(float)*4 },
    {HgiFormatInt32,       VK_FORMAT_R32_SINT,            sizeof(int32_t)*1 },
    {HgiFormatInt32Vec2,   VK_FORMAT_R32G32_SINT,         sizeof(int32_t)*2 },
    {HgiFormatInt32Vec3,   VK_FORMAT_R32G32B32_SINT,      sizeof(int32_t)*3 }
};

static const uint32_t
_SampleCountTable[][2] =
{
    {HgiSampleCount1,  VK_SAMPLE_COUNT_1_BIT},
    {HgiSampleCount4,  VK_SAMPLE_COUNT_4_BIT},
    {HgiSampleCount16, VK_SAMPLE_COUNT_16_BIT}
};

static const uint32_t
_ShaderStageTable[][2] =
{
    {HgiShaderStageVertex,   VK_SHADER_STAGE_VERTEX_BIT},
    {HgiShaderStageFragment, VK_SHADER_STAGE_FRAGMENT_BIT},
    {HgiShaderStageCompute,  VK_SHADER_STAGE_COMPUTE_BIT}
};

static const uint32_t
_TextureUsageTable[][2] =
{
    {HgiTextureUsageBitsColorTarget, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT},
    {HgiTextureUsageBitsDepthTarget, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT},
    {HgiTextureUsageBitsShaderRead,  VK_IMAGE_USAGE_SAMPLED_BIT},
    {HgiTextureUsageBitsShaderWrite, VK_IMAGE_USAGE_STORAGE_BIT},
    {HgiTextureUsageBitsTransferDst, VK_IMAGE_USAGE_TRANSFER_DST_BIT},
    {HgiTextureUsageBitsTransferSrc, VK_IMAGE_USAGE_TRANSFER_SRC_BIT},
};

static const uint32_t
_FormatFeatureTable[][2] =
{
    {HgiTextureUsageBitsColorTarget, VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT},
    {HgiTextureUsageBitsDepthTarget, VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT},
    {HgiTextureUsageBitsShaderRead,  VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT},
    {HgiTextureUsageBitsShaderWrite, VK_FORMAT_FEATURE_STORAGE_IMAGE_BIT},
    {HgiTextureUsageBitsTransferDst, VK_FORMAT_FEATURE_TRANSFER_DST_BIT},
    {HgiTextureUsageBitsTransferSrc, VK_FORMAT_FEATURE_TRANSFER_SRC_BIT},
};

static const uint32_t
_BufferUsageTable[][2] =
{
    {HgiBufferUsageUniform,     VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT},
    {HgiBufferUsageIndex16,     VK_BUFFER_USAGE_INDEX_BUFFER_BIT},
    {HgiBufferUsageIndex32,     VK_BUFFER_USAGE_INDEX_BUFFER_BIT},
    {HgiBufferUsageVertex,      VK_BUFFER_USAGE_VERTEX_BUFFER_BIT},
    {HgiBufferUsageStorage,     VK_BUFFER_USAGE_STORAGE_BUFFER_BIT},
    {HgiBufferUsageTransferSrc, VK_BUFFER_USAGE_TRANSFER_SRC_BIT},
    {HgiBufferUsageTransferDst, VK_BUFFER_USAGE_TRANSFER_DST_BIT},
};

static const uint32_t
_CullModeTable[HgiCullModeCount][2] =
{
    {HgiCullModeNone,         VK_CULL_MODE_NONE},
    {HgiCullModeFront,        VK_CULL_MODE_FRONT_BIT},
    {HgiCullModeBack,         VK_CULL_MODE_BACK_BIT},
    {HgiCullModeFrontAndBack, VK_CULL_MODE_FRONT_AND_BACK}
};

static const uint32_t
_PolygonModeTable[HgiPolygonModeCount][2] =
{
    {HgiPolygonModeFill,  VK_POLYGON_MODE_FILL},
    {HgiPolygonModeLine,  VK_POLYGON_MODE_LINE},
    {HgiPolygonModePoint, VK_POLYGON_MODE_POINT}
};

static const uint32_t
_WindingTable[HgiWindingCount][2] =
{
    {HgiWindingClockwise,        VK_FRONT_FACE_CLOCKWISE},
    {HgiWindingCounterClockwise, VK_FRONT_FACE_COUNTER_CLOCKWISE}
};

static const uint32_t
_CompareOpTable[HgiCompareCount][2] =
{
    {HgiCompareOpNever,          VK_COMPARE_OP_NEVER},
    {HgiCompareOpLess,           VK_COMPARE_OP_LESS},
    {HgiCompareOpEqual,          VK_COMPARE_OP_EQUAL},
    {HgiCompareOpLessOrEqual,    VK_COMPARE_OP_LESS_OR_EQUAL},
    {HgiCompareOpGreater,        VK_COMPARE_OP_GREATER},
    {HgiCompareOpNotEqual,       VK_COMPARE_OP_NOT_EQUAL},
    {HgiCompareOpGreaterOrEqual, VK_COMPARE_OP_GREATER_OR_EQUAL},
    {HgiCompareOpAlways,         VK_COMPARE_OP_ALWAYS}
};

static const uint32_t
_BindResourceTypeTable[HgiBindResourceTypeCount][2] =
{
    {HgiBindResourceTypeSampler,              VK_DESCRIPTOR_TYPE_SAMPLER},
    {HgiBindResourceTypeCombinedImageSampler, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER},
    {HgiBindResourceTypeSamplerImage,         VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE},
    {HgiBindResourceTypeStorageImage,         VK_DESCRIPTOR_TYPE_STORAGE_IMAGE},
    {HgiBindResourceTypeUniformBuffer,        VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER},
    {HgiBindResourceTypeStorageBuffer,        VK_DESCRIPTOR_TYPE_STORAGE_BUFFER}
};

VkFormat
HgiVkConversions::GetFormat(HgiFormat inFormat)
{
    if (!TF_VERIFY(inFormat!=HgiFormatInvalid)) return VK_FORMAT_UNDEFINED;
    return VkFormat(_FormatTable[inFormat][1]);
}

HgiFormat
HgiVkConversions::GetFormat(VkFormat inFormat)
{
    if (!TF_VERIFY(inFormat!=VK_FORMAT_UNDEFINED)) return HgiFormatInvalid;

    // While HdFormat/HgiFormat do not support BGRA channel ordering it may
    // be used for the native window swapchain on some platforms.
    if (inFormat == VK_FORMAT_B8G8R8A8_UNORM) return HgiFormatUNorm8Vec4;

    for (auto const& f : _FormatTable) {
        if (f[1] == inFormat) return HgiFormat(f[0]);
    }

    TF_CODING_ERROR("Missing format table entry");
    return HgiFormatInvalid;
}

uint32_t
HgiVkConversions::GetBytesPerPixel(HgiFormat inFormat)
{
    if (!TF_VERIFY(inFormat!=HgiFormatInvalid)) return 0;
    return VkFormat(_FormatTable[inFormat][2]);
}

VkImageAspectFlags
HgiVkConversions::GetImageAspectFlag(HgiTextureUsage usage)
{
    if (usage & HgiTextureUsageBitsDepthTarget) {
        return VK_IMAGE_ASPECT_DEPTH_BIT;
    }
    // XXX support for VK_IMAGE_ASPECT_STENCIL_BIT

    return VK_IMAGE_ASPECT_COLOR_BIT;
}

VkImageUsageFlags
HgiVkConversions::GetTextureUsage(HgiTextureUsage tu)
{
    VkImageUsageFlags vkFlags = 0;
    for (const auto& f : _TextureUsageTable) {
        if (tu & f[0]) vkFlags |= f[1];
    }

    if (vkFlags==0) {
        TF_CODING_ERROR("Missing texture usage table entry");
    }
    return vkFlags;
}

VkFormatFeatureFlags
HgiVkConversions::GetFormatFeature(HgiTextureUsage tu)
{
    VkFormatFeatureFlags vkFlags = 0;
    for (const auto& f : _FormatFeatureTable) {
        if (tu & f[0]) vkFlags |= f[1];
    }

    if (vkFlags==0) {
        TF_CODING_ERROR("Missing texture usage table entry");
    }
    return vkFlags;
}

VkAttachmentLoadOp
HgiVkConversions::GetLoadOp(HgiAttachmentLoadOp op)
{
    TF_VERIFY(op < HgiVkArraySize(_LoadOpTable));
    return VkAttachmentLoadOp(_LoadOpTable[op][1]);
}

VkAttachmentStoreOp
HgiVkConversions::GetStoreOp(HgiAttachmentStoreOp op)
{
    TF_VERIFY(op < HgiVkArraySize(_StoreOpTable));
    return VkAttachmentStoreOp(_StoreOpTable[op][1]);
}

VkSampleCountFlagBits
HgiVkConversions::GetSampleCount(HgiSampleCount sc)
{
    for (auto const& s : _SampleCountTable) {
        if (s[0] == sc) return VkSampleCountFlagBits(s[1]);
    }

    TF_CODING_ERROR("Missing Sample table entry");
    return VK_SAMPLE_COUNT_1_BIT;
}

VkShaderStageFlags
HgiVkConversions::GetShaderStages(HgiShaderStage ss)
{
    VkShaderStageFlags vkFlags = 0;
    for (const auto& f : _ShaderStageTable) {
        if (ss & f[0]) vkFlags |= f[1];
    }

    if (vkFlags==0) {
        TF_CODING_ERROR("Missing shader stage table entry");
    }
    return vkFlags;
}

VkBufferUsageFlags
HgiVkConversions::GetBufferUsage(HgiBufferUsage bu)
{
    VkBufferUsageFlags vkFlags = 0;
    for (const auto& f : _BufferUsageTable) {
        if (bu & f[0]) vkFlags |= f[1];
    }

    if (vkFlags==0) {
        TF_CODING_ERROR("Missing buffer usage table entry");
    }
    return vkFlags;
}

VkCullModeFlags
HgiVkConversions::GetCullMode(HgiCullMode cm)
{
    return VkCullModeFlags(_CullModeTable[cm][1]);
}

VkPolygonMode
HgiVkConversions::GetPolygonMode(HgiPolygonMode pm)
{
    return VkPolygonMode(_PolygonModeTable[pm][1]);
}

VkFrontFace
HgiVkConversions::GetWinding(HgiWinding wd)
{
    return VkFrontFace(_WindingTable[wd][1]);
}

VkCompareOp
HgiVkConversions::GetCompareOp(HgiCompareOp co)
{
    return VkCompareOp(_CompareOpTable[co][1]);
}

VkDescriptorType
HgiVkConversions::GetDescriptorType(HgiBindResourceType rt)
{
    return VkDescriptorType(_BindResourceTypeTable[rt][1]);
}

PXR_NAMESPACE_CLOSE_SCOPE
