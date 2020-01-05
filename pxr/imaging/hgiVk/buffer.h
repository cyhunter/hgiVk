#ifndef PXR_IMAGING_HGIVK_BUFFER_H
#define PXR_IMAGING_HGIVK_BUFFER_H

#include "pxr/imaging/hgi/buffer.h"

#include "pxr/imaging/hgiVk/api.h"
#include "pxr/imaging/hgiVk/vulkan.h"

PXR_NAMESPACE_OPEN_SCOPE

class HgiVkDevice;
class HgiVkCommandBuffer;


///
/// \class HgiVkBuffer
///
/// Vulkan implementation of HgiBuffer
///
class HgiVkBuffer final : public HgiBuffer {
public:
    HGIVK_API
    HgiVkBuffer(
        HgiVkDevice* device,
        HgiBufferDesc const& desc);

    HGIVK_API
    virtual ~HgiVkBuffer();

    HGIVK_API
    void UpdateBufferData(
        uint32_t byteOffset,
        size_t byteSize,
        const void* data) override;

public:
    /// Returns the vulkan buffer.
    HGIVK_API
    VkBuffer GetBuffer() const;

    /// Returns the descriptor of this buffer.
    HGIVK_API
    HgiBufferDesc const& GetDescriptor() const;

    /// Records a GPU->GPU copy command to copy the data from the provided
    /// source buffer into this (destination) buffer. This requires that the
    /// source buffer is setup as a staging buffer (HgiBufferUsageTransferSrc)
    /// and that this (destination) buffer has usage: HgiBufferUsageTransferDst.
    HGIVK_API
    void CopyBufferFrom(
        HgiVkCommandBuffer* cb,
        HgiVkBuffer const& src);

    /// Copy the entire contents of this buffer to the cpuDestBuffer.
    /// 'cpuDestBuffer' must be off minimum size: GetDescriptor().byteSize.
    /// The buffer must have usage flags HgiBufferUsageGpuToCpu or
    /// HgiBufferUsageCpuToGpu.
    HGIVK_API
    void CopyBufferTo(
        void* cpuDestBuffer);

private:
    HgiVkBuffer() = delete;
    HgiVkBuffer & operator=(const HgiVkBuffer&) = delete;
    HgiVkBuffer(const HgiVkBuffer&) = delete;

private:
    HgiVkDevice* _device;
    HgiBufferDesc _descriptor;
    VkBuffer _vkBuffer;
    VmaAllocation _vmaBufferAllocation;
    void* _dataMapped;
};


PXR_NAMESPACE_CLOSE_SCOPE

#endif
