#include "pxr/base/tf/diagnostic.h"

#include "pxr/imaging/hgiVk/buffer.h"
#include "pxr/imaging/hgiVk/commandBuffer.h"
#include "pxr/imaging/hgiVk/conversions.h"
#include "pxr/imaging/hgiVk/device.h"

PXR_NAMESPACE_OPEN_SCOPE


HgiVkBuffer::HgiVkBuffer(
    HgiVkDevice* device,
    HgiBufferDesc const& desc)
    : HgiBuffer(desc)
    , _device(device)
    , _descriptor(desc)
    , _vkBuffer(nullptr)
    , _vmaBufferAllocation(nullptr)
    , _dataMapped(nullptr)
{
    bool isStagingBuffer = (desc.usage & HgiBufferUsageTransferSrc);
    bool isDestinationBuffer = (desc.usage & HgiBufferUsageTransferDst);

    if (isStagingBuffer && desc.usage != HgiBufferUsageTransferSrc) {
        TF_CODING_ERROR("Buffer [%x] states it is HgiBufferUsageTransferSrc, "
                        "but has additional usage flags. Buffers that are used "
                        "as staging buffers (TransferSrc) must be used "
                        "exclusively for that purposes.", this);
    }

    VkBufferCreateInfo bufCreateInfo = {VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
    bufCreateInfo.size = desc.byteSize;
    bufCreateInfo.usage = HgiVkConversions::GetBufferUsage(desc.usage);
    bufCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE; // gfx queue only

    if (desc.data && !isStagingBuffer && !isDestinationBuffer) {
        // It is likely the caller intended for desc.data to be uploaded into
        // the buffer, but did not make this clear in the usage flags.
        // We should warn, because perhaps the data is accidental garbage.
        TF_WARN("Buffer [%x] descriptor provides data, but is missing "
                "HgiBufferUsageTransferDst in its usage flags.", this);
        bufCreateInfo.usage |= VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    }

    // https://gpuopen-librariesandsdks.github.io/
    // VulkanMemoryAllocator/html/usage_patterns.html

    // Create buffer with memory allocated and bound.
    // Equivalent to: vkCreateBuffer, vkAllocateMemory, vkBindBufferMemory
    VmaAllocationCreateInfo allocInfo = {};
    allocInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;

    // XXX On VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU it may be beneficial to
    // skip staging buffers and use VMA_MEMORY_USAGE_CPU_TO_GPU since all
    // memory is shared between CPU and GPU

    if (isStagingBuffer) {
        allocInfo.usage = VMA_MEMORY_USAGE_CPU_ONLY;
    } else if (desc.usage & HgiBufferUsageCpuToGpu) {
        // Read-backs are possible for GPU_TO_CPU, but are likely very slow.
        allocInfo.usage = VMA_MEMORY_USAGE_CPU_TO_GPU;
    } else if (desc.usage & HgiBufferUsageGpuToCpu) {
        allocInfo.usage = VMA_MEMORY_USAGE_GPU_TO_CPU;
    }

    // XXX On APPLE VMA_MEMORY_USAGE_CPU_TO_GPU may not work (textures only?).
    // I believe there is a bug with MoltenVK where we need to ensure
    // HOST_COHERENT is on the staging buffer. Without it the 'unmap' call below
    // causes a problem and the data never gets into the final buffer.
    // github.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator/issues/47
    // So far I've seen this happen with HgiVkBlitEncoder::CopyTextureGpuToCpu.
    // There we request a GpuToCpu buffer, but it never manages to fill the
    // buffer with the texture's pixels. Using VMA_MEMORY_USAGE_CPU_ONLY works.
    #if defined(__APPLE__)
        if (allocInfo.usage != VMA_MEMORY_USAGE_GPU_ONLY) {
            allocInfo.usage = VMA_MEMORY_USAGE_CPU_ONLY;
        }
    #endif

    TF_VERIFY(
        vmaCreateBuffer(
            _device->GetVulkanMemoryAllocator(),
            &bufCreateInfo,
            &allocInfo,
            &_vkBuffer,
            &_vmaBufferAllocation,
            nullptr) == VK_SUCCESS
    );

    // Persistently map the (HOST_VISIBLE) buffer
    if (allocInfo.usage != VMA_MEMORY_USAGE_GPU_ONLY) {
        TF_VERIFY(
            vmaMapMemory(
                _device->GetVulkanMemoryAllocator(),
                _vmaBufferAllocation,
                &_dataMapped) == VK_SUCCESS
        );
    }

    if (isStagingBuffer) {
        // Copy buffer data into host local staging buffer
        memcpy(_dataMapped, desc.data, desc.byteSize);

        // If the buffer was created with HOST_COHERENT we dont need to flush.
        // Only VMA_MEMORY_USAGE_CPU_ONLY guarantees this, please see comment
        // for vmaFlushAllocation in VulkanMemoryAllocator.
        if (allocInfo.usage != VMA_MEMORY_USAGE_CPU_ONLY) {
            vmaFlushAllocation(
                _device->GetVulkanMemoryAllocator(),
                _vmaBufferAllocation,
                0, // offset
                VK_WHOLE_SIZE);
        }
    }

    // Don't hold onto buffer data ptr locally. HgiBufferDesc states that:
    // "The application may alter or free this memory as soon as the constructor
    //  of the HgiTexture has returned."
    _descriptor.data = nullptr;
}

HgiVkBuffer::~HgiVkBuffer()
{
    if (_dataMapped) {
        vmaUnmapMemory(
            _device->GetVulkanMemoryAllocator(),
            _vmaBufferAllocation);
    }

    vmaDestroyBuffer(
        _device->GetVulkanMemoryAllocator(),
        _vkBuffer,
        _vmaBufferAllocation);
}


void
HgiVkBuffer::UpdateBufferData(
    uint32_t byteOffset,
    size_t byteSize,
    const void* data)
{
    if (!TF_VERIFY(_dataMapped, "Buffer is not HOST_VISIBLE")) return;

    // XXX Needs more testing. Does a copy succeed even with other usage
    // flags, but is just really slow? Or will is fail all together.
    TF_VERIFY(_descriptor.usage & HgiBufferUsageCpuToGpu,
              "Buffer [%x] usage is missing HgiBufferUsageCpuToGpu. "
              "UpdateBufferData may fail.", this);

    if (!TF_VERIFY(byteSize <= _descriptor.byteSize,
                   "Provided data too large for Buffer [%x].", this)) {
        return;
    }

    char* dest = (char*)_dataMapped;
    dest += byteOffset;
    memcpy(dest, data, byteSize);

    // We need to manually flush the persistent mapped buffer to make sure the
    // write is made visible to gpu.
    // See comments in VMA header (search for vmaFlushAllocation).
    // See also vkFlushMappedMemoryRanges (we don't need another barrier).
    vmaFlushAllocation(
        _device->GetVulkanMemoryAllocator(),
        _vmaBufferAllocation,
        byteOffset,
        (VkDeviceSize) byteSize
    );
}

VkBuffer
HgiVkBuffer::GetBuffer() const
{
    return _vkBuffer;
}

HgiBufferDesc const&
HgiVkBuffer::GetDescriptor() const
{
    return _descriptor;
}

void
HgiVkBuffer::CopyBufferFrom(
    HgiVkCommandBuffer* cb,
    HgiVkBuffer const& src)
{
    HgiBufferDesc const& srcDesc = src.GetDescriptor();
    bool isStagingBuffer = (srcDesc.usage & HgiBufferUsageTransferSrc);
    if (!isStagingBuffer) {
        TF_CODING_ERROR("Buffer [%x] is missing usage flag: "
                        "HgiBufferUsageTransferSrc", &src);
        return;
    }

    bool isDestSrc = (_descriptor.usage & HgiBufferUsageTransferDst);
    if (!isDestSrc) {
        TF_CODING_ERROR("Buffer [%x] is missing usage flag: "
                        "HgiBufferUsageTransferDst", this);
        return;
    }

    if (srcDesc.byteSize > _descriptor.byteSize) {
        TF_CODING_ERROR("Buffer src [%x] is larger than dest buffer [%x].",
                        &src, this);
        return;
    }

    // Copy data from staging buffer to destination (gpu) buffer
    VkBufferCopy copyRegion = {};
    copyRegion.srcOffset = 0;
    copyRegion.dstOffset = 0;
    copyRegion.size = srcDesc.byteSize;
    vkCmdCopyBuffer(
        cb->GetCommandBufferForRecoding(),
        src.GetBuffer(),
        _vkBuffer,
        1, // regionCount
        &copyRegion);

    // Make sure copy finishes before the next draw call.
    // XXX Optimization opportunity: Currently we always set vertex/index
    // as the consumer stage, but some buffers may be used later, such as an
    // SSBO used only in the fragment stage.
    VkBufferMemoryBarrier barrier = {VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER};
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT; // what producer does
    barrier.dstAccessMask = VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT |
                            VK_ACCESS_INDEX_READ_BIT;     // what consumer does
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.buffer = _vkBuffer;
    barrier.size = VK_WHOLE_SIZE;

    vkCmdPipelineBarrier(
        cb->GetCommandBufferForRecoding(),
        VK_PIPELINE_STAGE_TRANSFER_BIT,     // producer stage
        VK_PIPELINE_STAGE_VERTEX_INPUT_BIT, // consumer stage
        0, 0, nullptr, 1, &barrier, 0, nullptr);
}

void
HgiVkBuffer::CopyBufferTo(
    void* cpuDestBuffer)
{
    if (!TF_VERIFY(cpuDestBuffer, "Invalid dest buffer")) return;
    if (!TF_VERIFY(_dataMapped, "Buffer is not HOST_VISIBLE")) return;
    memcpy(cpuDestBuffer, _dataMapped, _descriptor.byteSize);
}


PXR_NAMESPACE_CLOSE_SCOPE
