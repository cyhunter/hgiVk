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
#ifndef PXR_IMAGING_HGI_GRAPHICS_ENCODER_H
#define PXR_IMAGING_HGI_GRAPHICS_ENCODER_H

#include <memory>

#include "pxr/pxr.h"
#include "pxr/base/gf/vec4i.h"
#include "pxr/imaging/hgi/api.h"
#include "pxr/imaging/hgi/buffer.h"
#include "pxr/imaging/hgi/encoderOps.h"
#include "pxr/imaging/hgi/enums.h"
#include "pxr/imaging/hgi/pipeline.h"
#include "pxr/imaging/hgi/resourceBindings.h"

PXR_NAMESPACE_OPEN_SCOPE

typedef std::unique_ptr<class HgiGraphicsEncoder> HgiGraphicsEncoderUniquePtr;


/// \class HgiGraphicsEncoder
///
/// A graphics API independent abstraction of graphics commands.
/// HgiGraphicsEncoder is a lightweight object that cannot be re-used after
/// EndEncoding. New encoders should be acquired each frame.
/// This encoder should only be used in the thread that created it.
///
class HgiGraphicsEncoder
{
public:
    HGI_API
    HgiGraphicsEncoder();

    HGI_API
    virtual ~HgiGraphicsEncoder();

    /// Finish recording of commands. No further commands can be recorded.
    HGI_API
    virtual void EndEncoding() = 0;

    /// Set viewport [left, BOTTOM, width, height] - OpenGL coords
    HGI_API
    virtual void SetViewport(GfVec4i const& vp) = 0;

    /// Only pixels that lie within the scissor box are modified by
    /// drawing commands.
    HGI_API
    virtual void SetScissor(GfVec4i const& sc) = 0;

    /// Bind a pipeline state object. Usually you call this right after calling
    /// CreateGraphicsEncoder to set the graphics pipeline state.
    /// The resource bindings used when creating the pipeline must be compatible
    /// with the resources bound via BindResources().
    HGI_API
    virtual void BindPipeline(HgiPipelineHandle pipeline) = 0;

    /// Bind resources such as textures and uniform buffers.
    /// Usually you call this right after BindPipeline() and the resources bound
    /// must be compatible with the bound pipeline.
    HGI_API
    virtual void BindResources(HgiResourceBindingsHandle resources) = 0;

    /// Binds the vertex buffer(s) that describe the vertex attributes.
    HGI_API
    virtual void BindVertexBuffers(HgiBufferHandleVector const& vertexBuffers) = 0;

    /// Set Push / Function constants.
    /// `resources` is the set that you are binding before the draw call. It
    /// describes the push constants you are about to set the value of.
    /// `stages` describes for what shader stage you are setting the push
    /// constant values for. Each stage can have its own (or none) pushConstants
    /// and they must match both what is described in the resource binding and
    /// the shader functions.
    /// `byteOffset` is the start offset in the push constants block of where
    /// you are updating the values. This value would be 0 if you are updating
    /// the entire push constants block with new data.
    /// `byteSize` is the size of the data you are updating.
    /// `data` is the data you are copying into the push constants block.
    HGI_API
    virtual void SetConstantValues(
        HgiResourceBindingsHandle resources,
        HgiShaderStage stages,
        uint32_t byteOffset,
        uint32_t byteSize,
        const void* data) = 0;

    /// Records a draw command that renders one or more instances of primitives
    /// using an indexBuffer starting from the base vertex of the base instance.
    /// `indexCount` is the number of vertices.
    /// `indexBufferByteOffset`: Byte offset within indexBuffer to start reading
    ///                          indices from.
    /// `firstIndex`: base index within the index buffer (usually 0).
    /// `vertexOffset`: the value added to the vertex index before indexing into
    ///                 the vertex buffer (baseVertex).
    /// `instanceCount`: number of instances (min 1) of the primitves to render.
    /// `firstInstance`: instance ID of the first instance to draw (usually 0).
    HGI_API
    virtual void DrawIndexed(
        HgiBufferHandle const& indexBuffer,
        uint32_t indexCount,
        uint32_t indexBufferByteOffset,
        uint32_t firstIndex,
        uint32_t vertexOffset,
        uint32_t instanceCount,
        uint32_t firstInstance) = 0;

    /// Push a debug marker onto the encoder.
    HGI_API
    virtual void PushDebugGroup(const char* label) = 0;

    /// Pop the lastest debug marker off encoder.
    HGI_API
    virtual void PopDebugGroup() = 0;

    /// Push a time query onto encoder. This records the start time.
    /// Timer results can be retrieved via Hgi::GetTimeQueries().
    HGI_API
    virtual void PushTimeQuery(const char* name) = 0;

    /// Pop last time query of encoder. This records the end time.
    /// Timer results can be retrieved via Hgi::GetTimeQueries().
    HGI_API
    virtual void PopTimeQuery() = 0;

private:
    HgiGraphicsEncoder & operator=(const HgiGraphicsEncoder&) = delete;
    HgiGraphicsEncoder(const HgiGraphicsEncoder&) = delete;
};


PXR_NAMESPACE_CLOSE_SCOPE

#endif
