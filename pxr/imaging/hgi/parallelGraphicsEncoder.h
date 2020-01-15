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
#ifndef PXR_IMAGING_HGI_PARALLEL_GRAPHICS_ENCODER_H
#define PXR_IMAGING_HGI_PARALLEL_GRAPHICS_ENCODER_H

#include <memory>

#include "pxr/pxr.h"
#include "pxr/imaging/hgi/api.h"
#include "pxr/imaging/hgi/graphicsEncoder.h"


PXR_NAMESPACE_OPEN_SCOPE

typedef std::unique_ptr<class HgiParallelGraphicsEncoder>
    HgiParallelGraphicsEncoderUniquePtr;


/// \class HgiParallelGraphicsEncoder
///
/// This encoder is used to split render work for a single render pass into
/// multiple threads / encoders.
/// The parallel encoder ensures that the load and store ops happen once at the
/// start and end of the entire render pass (instead of in each worker thread).
///
/// HgiParallelGraphicsEncoder cannot be re-used after EndEncoding.
///
class HgiParallelGraphicsEncoder
{
public:
    HGI_API
    HgiParallelGraphicsEncoder(const char* debugName=nullptr);

    HGI_API
    virtual ~HgiParallelGraphicsEncoder();

    /// Create a new graphics encoder for a worker thread.
    /// This should be called in the worker thread that uses the encoder.
    HGI_API
    virtual HgiGraphicsEncoderUniquePtr CreateGraphicsEncoder() = 0;

    /// Finish parallel recording of commands.
    /// This function must be called in the thread that constructed the parallel
    /// encoder, after all parallel work has completed.
    HGI_API
    virtual void EndEncoding() = 0;

private:
    HgiParallelGraphicsEncoder & operator=(
        const HgiParallelGraphicsEncoder&) = delete;

    HgiParallelGraphicsEncoder(
        const HgiParallelGraphicsEncoder&) = delete;
};


PXR_NAMESPACE_CLOSE_SCOPE

#endif
