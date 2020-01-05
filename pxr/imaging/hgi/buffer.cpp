#include "pxr/imaging/hgi/buffer.h"

PXR_NAMESPACE_OPEN_SCOPE

HgiBuffer::HgiBuffer(HgiBufferDesc const& desc)
{
}

HgiBuffer::~HgiBuffer()
{
}

HgiBufferDesc::HgiBufferDesc()
    : usage(HgiBufferUsageStorage)
    , byteSize(0)
    , data(nullptr)
{
}

bool operator==(
    const HgiBufferDesc& lhs,
    const HgiBufferDesc& rhs)
{
    return lhs.usage == rhs.usage &&
           lhs.byteSize == rhs.byteSize
           // lhs.data == rhs.data &&    // Intentionally omitted
    ;
}

bool operator!=(
    const HgiBufferDesc& lhs,
    const HgiBufferDesc& rhs)
{
    return !(lhs == rhs);
}

PXR_NAMESPACE_CLOSE_SCOPE
