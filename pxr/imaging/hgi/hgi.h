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
#ifndef PXR_IMAGING_HGI_HGI_H
#define PXR_IMAGING_HGI_HGI_H

#include "pxr/pxr.h"
#include "pxr/imaging/hgi/api.h"
#include "pxr/imaging/hgi/blitEncoder.h"
#include "pxr/imaging/hgi/graphicsEncoder.h"
#include "pxr/imaging/hgi/graphicsEncoderDesc.h"
#include "pxr/imaging/hgi/parallelGraphicsEncoder.h"
#include "pxr/imaging/hgi/pipeline.h"
#include "pxr/imaging/hgi/resourceBindings.h"
#include "pxr/imaging/hgi/shaderFunction.h"
#include "pxr/imaging/hgi/shaderProgram.h"
#include "pxr/imaging/hgi/texture.h"
#include "pxr/imaging/hgi/types.h"

PXR_NAMESPACE_OPEN_SCOPE


/// \class Hgi
///
/// Hydra Graphics Interface.
/// Hgi is used to communicate with one or more physical gpu devices.
///
/// Hgi provides API to create/destroy resources that a gpu device owns.
/// The lifetime of resources is not managed by Hgi, so it is up to the caller
/// to destroy resources and ensure those resources are no longer used.
///
/// Commands are recorded via an encoders.
///
class Hgi
{
public:
    HGI_API
    Hgi();

    HGI_API
    virtual ~Hgi();

    /// End current frame of rendering. Should be called exactly once per
    /// application frame. If there are multiple hydra's / viewports, EndFrame
    /// should only be called once after all hydra's have finished rendering.
    ///
    /// Commits all command buffers and prepares for next frame of rendering.
    /// The reason it should immediately prepare the next frame is that calls to
    /// Hgi may happen outside of the HdEngine::Execute cycle. For example the
    /// scene delegate may delete a rprim which immediately calls Finalize.
    /// So there is no clear 'BeginFrame' stage and Hgi must always be in a
    /// 'ready to record commands' state.
    HGI_API
    virtual void EndFrame() = 0;

    //
    // Command encoders
    //

    /// Returns a graphics encoder for temporary use that is ready to
    /// execute draw commands. GraphicsEncoder is a lightweight object that
    /// should be re-acquired each frame (don't hold onto it after EndEncoding).
    /// This encoder should only be used in the thread that created it.
    HGI_API
    virtual HgiGraphicsEncoderUniquePtr CreateGraphicsEncoder(
        HgiGraphicsEncoderDesc const& desc) = 0;

    /// Returns a parallel graphics encoder that can be used during parallel
    /// rendering of graphics jobs. ParallelGraphicsEncoder is a lightweight
    /// object that should be re-acquired each frame (don't hold onto it).
    /// You must also provide the pipeline object you plan to bind in each
    /// of the graphics encoders. (You must still bind it yourself).
    HGI_API
    virtual HgiParallelGraphicsEncoderUniquePtr CreateParallelGraphicsEncoder(
        HgiGraphicsEncoderDesc const& desc,
        HgiPipelineHandle pipeline) = 0;

    /// Returns a blit encoder for temporary use that is ready to execute
    /// resource copy commands. BlitEncoder is a lightweight object that
    /// should be re-acquired each frame (don't hold onto it after EndEncoding).
    /// This blit encoder can only be used in a single thread.
    HGI_API
    virtual HgiBlitEncoderUniquePtr CreateBlitEncoder() = 0;

    //
    // Resource API
    //

    /// Create a texture in rendering backend.
    HGI_API
    virtual HgiTextureHandle CreateTexture(HgiTextureDesc const & desc) = 0;

    /// Destroy a texture in rendering backend.
    HGI_API
    virtual void DestroyTexture(HgiTextureHandle* texHandle) = 0;

    /// Create a new buffer object
    HGI_API
    virtual HgiBufferHandle CreateBuffer(HgiBufferDesc const& desc) = 0;

    /// Destroy a buffer object
    HGI_API
    virtual void DestroyBuffer(HgiBufferHandle* bufferHandle) = 0;

    /// Create a new pipeline state object
    HGI_API
    virtual HgiPipelineHandle CreatePipeline(
        HgiPipelineDesc const& pipeDesc) = 0;

    /// Destroy a pipeline state object
    HGI_API
    virtual void DestroyPipeline(HgiPipelineHandle* pipeHandle) = 0;

    /// Create a new resource binding object
    HGI_API
    virtual HgiResourceBindingsHandle CreateResourceBindings(
        HgiResourceBindingsDesc const& desc) = 0;

    /// Destroy a resource binding object
    HGI_API
    virtual void DestroyResourceBindings(
        HgiResourceBindingsHandle* resHandle) = 0;

    /// Create a new shader function
    HGI_API
    virtual HgiShaderFunctionHandle CreateShaderFunction(
        HgiShaderFunctionDesc const& desc) = 0;

    /// Destroy a shader function
    HGI_API
    virtual void DestroyShaderFunction(
        HgiShaderFunctionHandle* shaderFunctionHandle) = 0;

    /// Create a new shader program
    HGI_API
    virtual HgiShaderProgramHandle CreateShaderProgram(
        HgiShaderProgramDesc const& desc) = 0;

    /// Destroy a shader program
    HGI_API
    virtual void DestroyShaderProgram(
        HgiShaderProgramHandle* shaderProgramHandle) = 0;

    //
    // Query API
    //

    /// Returns used and unused memmory (in bytes)
    HGI_API
    virtual void GetMemoryInfo(size_t* used, size_t* unused) = 0;

private:
    Hgi & operator=(const Hgi&) = delete;
    Hgi(const Hgi&) = delete;
};

PXR_NAMESPACE_CLOSE_SCOPE

#endif
