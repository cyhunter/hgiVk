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
#ifndef PXR_IMAGING_HGI_COMPUTE_ENCODER_H
#define PXR_IMAGING_HGI_COMPUTE_ENCODER_H

#include <memory>

#include "pxr/pxr.h"
#include "pxr/base/gf/vec4i.h"
#include "pxr/imaging/hgi/api.h"
#include "pxr/imaging/hgi/buffer.h"
#include "pxr/imaging/hgi/enums.h"
#include "pxr/imaging/hgi/pipeline.h"
#include "pxr/imaging/hgi/resourceBindings.h"

PXR_NAMESPACE_OPEN_SCOPE

typedef std::unique_ptr<class HgiComputeEncoder> HgiComputeEncoderUniquePtr;


/// \class HgiComputeEncoder
///
/// A graphics API independent abstraction of compute commands.
/// HgiComputeEncoder is a lightweight object that cannot be re-used after
/// EndEncoding. New encoders should be acquired each frame.
/// This encoder should only be used in the thread that created it.
///
class HgiComputeEncoder
{
public:
    HGI_API
    HgiComputeEncoder();

    HGI_API
    virtual ~HgiComputeEncoder();

    /// Finish recording of commands. No further commands can be recorded.
    HGI_API
    virtual void EndEncoding() = 0;

    /// Bind a pipeline state object. Usually you call this right after calling
    /// CreateComputeEncoder to set the compute pipeline state.
    /// The resource bindings used when creating the pipeline must be compatible
    /// with the resources bound via BindResources().
    HGI_API
    virtual void BindPipeline(HgiPipelineHandle pipeline) = 0;

    /// Bind resources such as textures and storage buffers.
    /// Usually you call this right after BindPipeline() and the resources bound
    /// must be compatible with the bound pipeline.
    HGI_API
    virtual void BindResources(HgiResourceBindingsHandle resources) = 0;

    /// Execute a compute shader with provided thread group count in each
    /// dimension.
    HGI_API
    virtual void Dispatch(
        uint32_t threadGrpCntX,
        uint32_t threadGrpCntY,
        uint32_t threadGrpCntZ) = 0;

    /// Push a debug marker onto the encoder.
    HGI_API
    virtual void PushDebugGroup(const char* label) = 0;

    /// Pop the lastest debug marker off encoder.
    HGI_API
    virtual void PopDebugGroup() = 0;

private:
    HgiComputeEncoder & operator=(const HgiComputeEncoder&) = delete;
    HgiComputeEncoder(const HgiComputeEncoder&) = delete;
};


PXR_NAMESPACE_CLOSE_SCOPE

#endif
