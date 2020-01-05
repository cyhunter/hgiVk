//
// Copyright 2019 Pixar
//
// Licensed under the Apache License, Version 2.0 (the "Apache License")
// with the following modification; you may not use this file except in
// compliance with the Apache License and the following modification to it:
// Section 6. Trademarks. is deleted and replaced with:
//
// 6. Trademarks. This License does not grant permission to use the trade
//    names, trademarks, service marks, or product names of the Licensor
//    and its affiliates, except as required to comply with Section 4(c) of
//    the License and to reproduce the content of the NOTICE file.
//
// You may obtain a copy of the Apache License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the Apache License with the above modification is
// distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
// KIND, either express or implied. See the Apache License for the specific
// language governing permissions and limitations under the Apache License.
//
#ifndef PXR_IMAGING_HGI_ENUMS_H
#define PXR_IMAGING_HGI_ENUMS_H

#include "pxr/pxr.h"
#include "pxr/imaging/hgi/api.h"
#include <cstdint>

PXR_NAMESPACE_OPEN_SCOPE

typedef uint32_t HgiBits;


/// \enum HgiDeviceCapabilitiesBits
///
/// Describes what capabilities the requested device must have.
///
/// <ul>
/// <li>HgiDeviceCapabilitiesBitsPresentation:
///   The device must be capable of presenting graphics to screen</li>
/// </ul>
///
enum HgiDeviceCapabilitiesBits : HgiBits {
    HgiDeviceCapabilitiesBitsPresentation = 1 << 0,
};

typedef HgiBits HgiDeviceCapabilities;


/// \enum HgiTextureUsageBits
///
/// Describes how the texture will be used. If a texture has multiple uses you
/// can combine multiple bits.
///
/// <ul>
/// <li>HgiTextureUsageBitsUndefined:
///   Invalid texture.</li>
/// <li>HgiTextureUsageBitsColorTarget:
///   The texture is an color attachment rendered into via a render pass.</li>
/// <li>HgiTextureUsageBitsDepthTarget:
///   The texture is an depth attachment rendered into via a render pass.</li>
/// <li>HgiTextureUsageBitsShaderRead:
///   The texture is sampled from in a shader (image load / sampling)</li>
/// <li>HgiTextureUsageBitsShaderWrite:
///   The texture is written into from in a shader (image store)</li>
/// <li>HgiTextureUsageBitsTransferDst:
///   The texture may be a write-dest of a transfer / copy cmd.
///   Eg. You need this when uploading pixels to a texture (disk image).</li>
/// <li>HgiTextureUsageBitsTransferSrc:
///   The texture may be a source of a transfer / copy cmd.
///   You need this when the pixel data of the texture is copied to a buffer,
///   for example when reading the primId aov for picking.</li>
/// <li>HgiTextureUsageBitsSwapchain:
///   The texture is used as an attachment of the swapchain.</li>
/// <li>HgiTextureUsageBitsBGRA:
///   This bit is present if the texture uses reversed channel ordering.
///   Both HdFormat and HgiFormat do not support such a format, but it may be
///   needed on some platforms that only support BGRA window swapchains.</li>
/// </ul>
///
enum HgiTextureUsageBits : HgiBits {
    HgiTextureUsageBitsUndefined   = 0,
    HgiTextureUsageBitsColorTarget = 1 << 0,
    HgiTextureUsageBitsDepthTarget = 1 << 1,
    HgiTextureUsageBitsShaderRead  = 1 << 2,
    HgiTextureUsageBitsShaderWrite = 1 << 3,
    HgiTextureUsageBitsTransferDst = 1 << 4,
    HgiTextureUsageBitsTransferSrc = 1 << 5,
    HgiTextureUsageBitsSwapchain   = 1 << 6,
    HgiTextureUsageBitsBGRA        = 1 << 7,
};

typedef HgiBits HgiTextureUsage;


/// \enum HgiSampleCount
///
/// Sample count for multi-sampling
///
enum HgiSampleCount {
    HgiSampleCount1  = 1,
    HgiSampleCount4  = 4,
    HgiSampleCount16 = 16,
};


/// \enum HgiAttachmentLoadOp
///
/// Describes what will happen to the attachment pixel data prior to rendering.
///
/// <ul>
/// <li>HgiAttachmentLoadOpDontCare:
///   All pixels are rendered to. Pixel data in render target starts undefined.</li>
/// <li>HgiAttachmentLoadOpClear:
///   The attachment  pixel data is cleared to a specified color value.</li>
/// <li>HgiAttachmentLoadOpLoad:
///   Previous pixel data is loaded into attachment prior to rendering.</li>
/// </ul>
///
enum HgiAttachmentLoadOp {
    HgiAttachmentLoadOpDontCare = 0,
    HgiAttachmentLoadOpClear,
    HgiAttachmentLoadOpLoad,
};


/// \enum HgiAttachmentStoreOp
///
/// Describes what will happen to the attachment pixel data after rendering.
///
/// <ul>
/// <li>HgiAttachmentStoreOpDontCare:
///   Pixel data is undefined after rendering has completed (no store cost)</li>
/// <li>HgiAttachmentStoreOpStore:
///   The attachment pixel data is stored in memory.</li>
/// </ul>
///
enum HgiAttachmentStoreOp {
    HgiAttachmentStoreOpDontCare = 0,
    HgiAttachmentStoreOpStore,
};


/// \enum HgiPipelineType
///
/// Describes the intended bind point for this pipeline.
///
/// <ul>
/// <li>HgiPipelineTypeGraphics:
///   The pipeline is meant to be bound to the graphics pipeline.</li>
/// <li>HgiPipelineTypeCompute:
///   The pipeline is meant to be bound to the compute pipeline.</li>
/// </ul>
///
enum HgiPipelineType {
    HgiPipelineTypeGraphics = 0,
    HgiPipelineTypeCompute,

    HgiPipelineTypeCount
};


/// \enum HgiDepthStateBits
///
/// Describes depth state for a pipeline.
///
/// <ul>
/// <li>HgiDepthStateBitsDepthTest:
///   Enable depth test.</li>
/// <li>HgiBlendStateBitsDepthWrite:
///   Enable depth write</li>
/// <li>HgiBlendStateBitsStencilTest:
///   Enable Stencil test</li>
/// </ul>
///
enum HgiDepthStateBits : HgiBits {
    HgiDepthStateBitsDepthNone   = 0,
    HgiDepthStateBitsDepthTest   = 1 << 0,
    HgiBlendStateBitsDepthWrite  = 1 << 1,
    HgiBlendStateBitsStencilTest = 1 << 2,
};
typedef HgiBits HgiDepthState;


/// \enum HgiCompareOp
///
/// Describes compare operation (depth test)
///
enum HgiCompareOp {
    HgiCompareOpNever = 0,
    HgiCompareOpLess,
    HgiCompareOpEqual,
    HgiCompareOpLessOrEqual,
    HgiCompareOpGreater,
    HgiCompareOpNotEqual,
    HgiCompareOpGreaterOrEqual,
    HgiCompareOpAlways,

    HgiCompareCount
};


/// \enum HgiShaderStage
///
/// Describes the stage a shader function operates in.
///
/// <ul>
/// <li>HgiShaderStageVertex:
///   Vertex Shader.</li>
/// <li>HgiShaderStageFragment:
///   Fragment Shader.</li>
/// <li>HgiShaderStageCompute:
///   Compute Shader.</li>
/// </ul>
///
enum HgiShaderStageBits : HgiBits {
    HgiShaderStageVertex   = 1 << 0,
    HgiShaderStageFragment = 1 << 1,
    HgiShaderStageCompute  = 1 << 2
};
typedef HgiBits HgiShaderStage;


/// \enum HgiBufferUsageBits
///
/// Describes the properties and usage of the buffer.
///
/// Buffers are considered GPU_ONLY unless the buffers is a transfer buffer
/// (HgiBufferUsageTransferSrc). Or the buffer author specifies
/// HgiBufferUsageGpuToCpu or HgiBufferUsageCpuToGpu.
///
/// <ul>
/// <li>HgiBufferUsageUniform:
///   Shader uniform buffer (read only on GPU).
///   You likely need to add: HgiBufferUsageCpuToGpu.</li>
/// <li>HgiBufferUsageIndex16:
///   Topology 16 bit indices.</li>
/// <li>HgiBufferUsageIndex32:
///   Topology 32 bit indices.</li>
/// <li>HgiBufferUsageVertex:
///   Vertex attributes.</li>
/// <li>HgiBufferUsageStorage:
///   Shader storage buffer / Argument buffer.</li>
/// <li>HgiBufferUsageTransferSrc:
///   Buffer is source for a copy operation (staging buffer).</li>
/// <li>HgiBufferUsageTransferDst:
///   Buffer is destination of a copy operation (gpu->gpu copy).</li>
/// <li>HgiBufferUsageCpuToGpu:
///   Buffer is frequently (e.g. every frame or draw call) written to by CPU,
///   read-only on GPU. Examples: uniform buffer, storage buffer.
///   Cannot be combined with HgiBufferUsageGpuToCpu.</li>
/// <li>HgiBufferUsageGpuToCpu:
///   Buffer contain data written by GPU that you want to read back on CPU,
///   e.g. results of a computation.
///   Cannot be combined with HgiBufferUsageCpuToGpu.</li>
/// </ul>
///
enum HgiBufferUsageBits : HgiBits {
    HgiBufferUsageUniform     = 1 << 0,
    HgiBufferUsageIndex16     = 1 << 1,
    HgiBufferUsageIndex32     = 1 << 2,
    HgiBufferUsageVertex      = 1 << 3,
    HgiBufferUsageStorage     = 1 << 4,
    HgiBufferUsageTransferSrc = 1 << 5,
    HgiBufferUsageTransferDst = 1 << 6,
    HgiBufferUsageCpuToGpu    = 1 << 7,
    HgiBufferUsageGpuToCpu    = 1 << 8,
};
typedef HgiBits HgiBufferUsage;


/// \enum HgiPolygonMode
///
/// Controls polygon mode during rasterization
///
/// <ul>
/// <li>HgiPolygonModeFill:
///   Polygons are filled.</li>
/// <li>HgiPolygonModeLine:
///   Polygon edges are drawn as line segments.</li>
/// <li>HgiPolygonModePoint:
///   Polygon vertices are drawn as points.</li>
/// </ul>
///
enum HgiPolygonMode {
    HgiPolygonModeFill = 0,
    HgiPolygonModeLine,
    HgiPolygonModePoint,

    HgiPolygonModeCount
};


/// \enum HgiCullMode
///
/// Controls primitive (faces) culling.
///
/// <ul>
/// <li>HgiPolygonModeNone:
///   No primitive are discarded.</li>
/// <li>HgiPolygonModeFront:
///   Front-facing primitive are discarded.</li>
/// <li>HgiPolygonModeBack:
///   Back-facing primitive are discarded.</li>
/// <li>HgiPolygonModeFrontAndBack:
///   All primitive are discarded.</li>
/// </ul>
///
enum HgiCullMode {
    HgiCullModeNone = 0,
    HgiCullModeFront,
    HgiCullModeBack,
    HgiCullModeFrontAndBack,

    HgiCullModeCount
};


/// \enum HgiWinding
///
/// Determines the front-facing orientation of a primitive (face).
///
/// <ul>
/// <li>HgiWindingClockwise:
///   Primitives with clockwise vertex-order are front facing.</li>
/// <li>HgiWindingCounterClockwise:
///   Primitives with counter-clockwise vertex-order are front facing.</li>
/// </ul>
///
enum HgiWinding {
    HgiWindingClockwise = 0,
    HgiWindingCounterClockwise,

    HgiWindingCount
};


/// \enum HgiBindResourceType
///
/// Describes the type of the resource to be bound.
///
/// <ul>
/// <li>HgiBindResourceTypeSampler:
///   Sampler</li>
/// <li>HgiBindResourceTypeCombinedImageSampler:
///   Image and sampler combined in one.</li>
/// <li>HgiBindResourceTypeSamplerImage:
///   Image for use with sampling ops.</li>
/// <li>HgiBindResourceTypeStorageImage:
///   Storage image used for image store/load ops (Unordered Access View).</li>
/// <li>HgiBindResourceTypeUniformBuffer:
///   Uniform buffer (UBO).</li>
/// <li>HgiBindResourceTypeStorageBuffer:
///   Shader storage buffer (SSBO).</li>
/// </ul>
///
enum HgiBindResourceType {
    HgiBindResourceTypeSampler = 0,
    HgiBindResourceTypeCombinedImageSampler,
    HgiBindResourceTypeSamplerImage,
    HgiBindResourceTypeStorageImage,
    HgiBindResourceTypeUniformBuffer,
    HgiBindResourceTypeStorageBuffer,

    HgiBindResourceTypeCount
};


PXR_NAMESPACE_CLOSE_SCOPE

#endif
