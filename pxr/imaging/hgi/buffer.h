#ifndef PXR_IMAGING_HGI_BUFFER_H
#define PXR_IMAGING_HGI_BUFFER_H

#include <stdlib.h>
#include <vector>

#include "pxr/pxr.h"
#include "pxr/imaging/hgi/api.h"
#include "pxr/imaging/hgi/enums.h"
#include "pxr/imaging/hgi/types.h"


PXR_NAMESPACE_OPEN_SCOPE

struct HgiBufferDesc;


///
/// \class HgiBuffer
///
/// Represents a buffer (vertex, index, storage, etc)
///
class HgiBuffer {
public:
    HGI_API
    HgiBuffer(HgiBufferDesc const& desc);

    HGI_API
    virtual ~HgiBuffer();

    /// Update the buffer with new data (eg. uniform or shader storage buffers).
    /// This requires that the buffer was created with HgiBufferUsageCpuToGpu.
    /// Do not use this if the buffer only needs to receive data one time, for
    /// example a vertex buffer. For one-time upload use `HgiBufferDesc.data`
    /// during buffer construction.
    /// Note that UpdateBufferData happens 'immediately'. It is up to the caller
    /// to ensure that the GPU is not currently consuming the portion of the
    /// buffer that is being updated. E.g. tripple-buffer/cycle between several
    /// buffer objects. Or make a buffer that is 3x the needed size and cycle
    /// between portions of the buffer.
    HGI_API
    virtual void UpdateBufferData(
        uint32_t byteOffset,
        size_t byteSize,
        const void* data) = 0;

private:
    HgiBuffer() = delete;
    HgiBuffer & operator=(const HgiBuffer&) = delete;
    HgiBuffer(const HgiBuffer&) = delete;
};

typedef HgiBuffer* HgiBufferHandle;
typedef std::vector<HgiBufferHandle> HgiBufferHandleVector;



/// \struct HgiBufferDesc
///
/// Describes the properties needed to create a GPU buffer.
///
/// <ul>
/// <li>usage:
///   Bits describing the intended usage and properties of the buffer.</li>
/// <li>byteSize:
///   Byte size (length) of buffer</li>
/// <li>data:
///   CPU pointer to initialization data of buffer.
///   The memory is consumed immediately during the creation of the HgiBuffer.
///   The application may alter or free this memory as soon as the constructor
///   of the HgiBuffer has returned.</li>
/// </ul>
///
struct HgiBufferDesc {
    HGI_API
    HgiBufferDesc();

    HgiBufferUsage usage;
    size_t byteSize;
    void const* data;
};

HGI_API
bool operator==(
    const HgiBufferDesc& lhs,
    const HgiBufferDesc& rhs);

HGI_API
bool operator!=(
    const HgiBufferDesc& lhs,
    const HgiBufferDesc& rhs);


PXR_NAMESPACE_CLOSE_SCOPE

#endif
