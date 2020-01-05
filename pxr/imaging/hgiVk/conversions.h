#ifndef PXR_IMAGING_HGIVK_CONVERSIONS_H
#define PXR_IMAGING_HGIVK_CONVERSIONS_H

#include "pxr/pxr.h"
#include "pxr/imaging/hgi/enums.h"
#include "pxr/imaging/hgi/types.h"

#include "pxr/imaging/hgi/enums.h"

#include "pxr/imaging/hgiVk/api.h"
#include "pxr/imaging/hgiVk/vulkan.h"

PXR_NAMESPACE_OPEN_SCOPE

///
/// \class HgiVkConversions
///
/// Converts from Hgi types to Vulkan types.
///
class HgiVkConversions final
{
public:
    HGIVK_API
    static VkFormat GetFormat(HgiFormat inFormat);

    HGIVK_API
    static HgiFormat GetFormat(VkFormat inFormat);

    HGIVK_API
    static uint32_t GetBytesPerPixel(HgiFormat inFormat);

    HGIVK_API
    static VkImageAspectFlags GetImageAspectFlag(HgiTextureUsage usage);

    HGIVK_API
    static VkImageUsageFlags GetTextureUsage(HgiTextureUsage tu);

    HGIVK_API
    static VkFormatFeatureFlags GetFormatFeature(HgiTextureUsage tu);

    HGIVK_API
    static VkAttachmentLoadOp GetLoadOp(HgiAttachmentLoadOp op);

    HGIVK_API
    static VkAttachmentStoreOp GetStoreOp(HgiAttachmentStoreOp op);

    HGIVK_API
    static VkSampleCountFlagBits GetSampleCount(HgiSampleCount sc);

    HGIVK_API
    static VkShaderStageFlags GetShaderStages(HgiShaderStage ss);

    HGIVK_API
    static VkBufferUsageFlags GetBufferUsage(HgiBufferUsage bu);

    HGIVK_API
    static VkCullModeFlags GetCullMode(HgiCullMode cm);

    HGIVK_API
    static VkPolygonMode GetPolygonMode(HgiPolygonMode pm);

    HGIVK_API
    static VkFrontFace GetWinding(HgiWinding wd);

    HGIVK_API
    static VkCompareOp GetCompareOp(HgiCompareOp co);

    HGIVK_API
    static VkDescriptorType GetDescriptorType(HgiBindResourceType rt);
};


PXR_NAMESPACE_CLOSE_SCOPE

#endif

