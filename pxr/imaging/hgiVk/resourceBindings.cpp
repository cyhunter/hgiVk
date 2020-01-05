#include "pxr/imaging/hgiVk/buffer.h"
#include "pxr/imaging/hgiVk/commandBuffer.h"
#include "pxr/imaging/hgiVk/conversions.h"
#include "pxr/imaging/hgiVk/device.h"
#include "pxr/imaging/hgiVk/resourceBindings.h"
#include "pxr/imaging/hgiVk/texture.h"
#include "pxr/imaging/hgiVk/vulkan.h"

PXR_NAMESPACE_OPEN_SCOPE

HgiVkResourceBindings::HgiVkResourceBindings(
    HgiVkDevice* device,
    HgiResourceBindingsDesc const& desc)
    : HgiResourceBindings(desc)
    , _device(device)
    , _descriptor(desc)
    , _vkDescriptorSetLayout(nullptr)
    , _vkDescriptorSet(nullptr)
    , _vkPipelineLayout(nullptr)
{
    // initialize the pool sizes for each descriptor type we support
    std::vector<VkDescriptorPoolSize> poolSizes;
    poolSizes.resize(HgiBindResourceTypeCount);

    for (size_t i=0; i<HgiBindResourceTypeCount; i++) {
        HgiBindResourceType bt = HgiBindResourceType(i);
        VkDescriptorPoolSize p;
        p.descriptorCount = 0;
        p.type = HgiVkConversions::GetDescriptorType(bt);
        poolSizes[i] = p;
    }

    //
    // Create DescriptorSetLayout to describe resource bindings
    //
    // The descriptors are reference by shader code. E.g.
    //   layout (set=S, binding=B) uniform sampler2D...
    //   layout (std140, binding = 0) uniform buffer{}
    //
    std::vector<VkDescriptorSetLayoutBinding> bindings;

    for (HgiTextureBindDesc const& t : desc.textures) {
        VkDescriptorSetLayoutBinding d = {};
        d.binding = t.bindingIndex;
        d.descriptorType = HgiVkConversions::GetDescriptorType(t.resourceType);
        poolSizes[t.resourceType].descriptorCount++;
        d.descriptorCount = (uint32_t) t.textures.size();
        d.stageFlags = HgiVkConversions::GetShaderStages(t.stageUsage);
        d.pImmutableSamplers = nullptr;
        bindings.emplace_back(std::move(d));
    }

    for (HgiBufferBindDesc const& b : desc.buffers) {
        VkDescriptorSetLayoutBinding d = {};
        d.binding = b.bindingIndex;
        d.descriptorType = HgiVkConversions::GetDescriptorType(b.resourceType);
        poolSizes[b.resourceType].descriptorCount++;
        d.descriptorCount = (uint32_t) b.buffers.size();
        d.stageFlags = HgiVkConversions::GetShaderStages(b.stageUsage);
        d.pImmutableSamplers = nullptr;
        bindings.emplace_back(std::move(d));
    }

    VkDescriptorSetLayoutCreateInfo setCreateInfo =
        {VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
    setCreateInfo.bindingCount = (uint32_t) bindings.size();
    setCreateInfo.pBindings = bindings.data();

    TF_VERIFY(
        vkCreateDescriptorSetLayout(
            _device->GetVulkanDevice(),
            &setCreateInfo,
            HgiVkAllocator(),
            &_vkDescriptorSetLayout) == VK_SUCCESS
    );

    //
    // Create the descriptor pool.
    //
    // XXX For now each resource bindings gets its own pool to allocate its
    // descriptor set from. We can't have a global descriptor pool since we have
    // multiple threads creating resourceBindings. On top of that, when the
    // resourceBindings gets destroyed it must de-allocate its descriptor set
    // in the correct descriptor pool (which is different than command buffers
    // where the entire pool is reset at the beginning of a new frame).
    //
    // If having a descriptor pool per resourceBindings turns out to be too much
    // overhead (e.g. if many resourceBindings are created/destroyed each frame)
    // then we can consider an approach similar to GetThreadLocalCommandBuffer.
    // We could allocate larger descriptor pools per frame and per thread.
    //

    for (size_t i=poolSizes.size(); i-- > 0;) {
        // Remove empty descriptorPoolSize or vulkan validation will complain
        if (poolSizes[i].descriptorCount == 0) {
            std::iter_swap(poolSizes.begin() + i, poolSizes.end() - 1);
            poolSizes.pop_back();
        }
    }

    VkDescriptorPoolCreateInfo pool_info = {};
    pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    pool_info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    pool_info.maxSets = 1; // Each resourceBinding has own pool -- read above
    pool_info.poolSizeCount = (uint32_t) poolSizes.size();
    pool_info.pPoolSizes = poolSizes.data();

    TF_VERIFY(
        vkCreateDescriptorPool(
            _device->GetVulkanDevice(),
            &pool_info,
            HgiVkAllocator(),
            &_vkDescriptorPool) == VK_SUCCESS
    );

    //
    // Create Descriptor Set
    //
    VkDescriptorSetAllocateInfo allocateInfo =
        {VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};

    allocateInfo.descriptorPool = _vkDescriptorPool;
    allocateInfo.descriptorSetCount = 1;
    allocateInfo.pSetLayouts = &_vkDescriptorSetLayout;

    TF_VERIFY(
        vkAllocateDescriptorSets(
            _device->GetVulkanDevice(),
            &allocateInfo,
            &_vkDescriptorSet) == VK_SUCCESS
    );

    //
    // Textures
    //

    std::vector<VkWriteDescriptorSet> writeSets;

    // Array-of-textures platform limits:
    #if defined(__ANDROID__) // Android 9
        #define AF_DESCRIPTOR_CNT_MAX 79u
    #elif defined(__APPLE__) && defined(__MACH__) // macOS 10.14
        #define AF_DESCRIPTOR_CNT_MAX 128u
    #elif defined(__APPLE__) && defined(TARGET_OS_IPHONE) // iOS 12
        #define AF_DESCRIPTOR_CNT_MAX 31u
    #else
        #define AF_DESCRIPTOR_CNT_MAX 512u // Windows, Linux Intel 768, NV 65535
    #endif

    TF_VERIFY(desc.textures.size() < AF_DESCRIPTOR_CNT_MAX,
              "Array-of-texture size exceeded: %d",
              AF_DESCRIPTOR_CNT_MAX);

    _imageInfos.clear();

    for (size_t i=0; i<desc.textures.size(); i++) {
        HgiTextureBindDesc const& texDesc = desc.textures[i];

        TF_VERIFY(texDesc.textures.size() < AF_DESCRIPTOR_CNT_MAX,
                  "Array-of-texture size exceeded: %d", AF_DESCRIPTOR_CNT_MAX);

        for (HgiTextureHandle const& texHandle : texDesc.textures) {
            HgiVkTexture* tex = static_cast<HgiVkTexture*>(texHandle);
            if (!TF_VERIFY(tex)) continue;
            VkDescriptorImageInfo imageInfo;
            imageInfo.sampler = tex->GetSampler();
            imageInfo.imageLayout = tex->GetImageLayout();
            imageInfo.imageView = tex->GetImageView();
            _imageInfos.emplace_back(std::move(imageInfo));
        }

        VkWriteDescriptorSet writeSet= {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
        writeSet.dstBinding = texDesc.bindingIndex;
        writeSet.dstArrayElement = 0;
        writeSet.descriptorCount = (uint32_t) texDesc.textures.size();
        writeSet.dstSet = _vkDescriptorSet;
        writeSet.pBufferInfo = nullptr;
        writeSet.pImageInfo = _imageInfos.data();
        writeSet.pTexelBufferView = nullptr;
        writeSet.descriptorType =
            HgiVkConversions::GetDescriptorType(texDesc.resourceType);
        writeSets.emplace_back(std::move(writeSet));
    }

    //
    // Buffers
    //

    _bufferInfos.clear();

    for (size_t i=0; i<desc.buffers.size(); i++) {
        HgiBufferBindDesc const& bufDesc = desc.buffers[i];

        TF_VERIFY(bufDesc.buffers.size() == bufDesc.offsets.size());

        for (HgiBufferHandle const& bufHandle : bufDesc.buffers) {
            HgiVkBuffer* buf = static_cast<HgiVkBuffer*>(bufHandle);
            if (!TF_VERIFY(buf)) continue;
            VkDescriptorBufferInfo bufferInfo;
            bufferInfo.buffer = buf->GetBuffer();
            bufferInfo.offset = bufDesc.offsets[i];
            bufferInfo.range = VK_WHOLE_SIZE;
            _bufferInfos.emplace_back(std::move(bufferInfo));
        }

        VkWriteDescriptorSet writeSet= {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
        writeSet.dstBinding = bufDesc.bindingIndex;
        writeSet.dstArrayElement = 0;
        writeSet.descriptorCount = (uint32_t) bufDesc.buffers.size();
        writeSet.dstSet = _vkDescriptorSet;
        writeSet.pBufferInfo = _bufferInfos.data();
        writeSet.pImageInfo = nullptr;
        writeSet.pTexelBufferView = nullptr;
        writeSet.descriptorType =
            HgiVkConversions::GetDescriptorType(bufDesc.resourceType);
        writeSets.emplace_back(std::move(writeSet));
    }

    // Note: this update happens immediate. It is not recorded via a command.
    // This means we should only do this if the descriptorSet is not currently
    // in use on GPU.
    vkUpdateDescriptorSets(
        _device->GetVulkanDevice(),
        (uint32_t) writeSets.size(),
        writeSets.data(),
        0,        // copy count
        nullptr); // copy_desc

    //
    // Pipeline layout contains descriptor set layouts and push constant ranges.
    //

    std::vector<VkPushConstantRange> pcRanges;
    for (HgiPushConstantDesc const& pcDesc : desc.pushConstants) {
        TF_VERIFY(pcDesc.byteSize % 4 == 0, "Push constants not multipes of 4");
        VkPushConstantRange pushConstantRange = {};
        pushConstantRange.offset = pcDesc.offset;
        pushConstantRange.size = pcDesc.byteSize;
        pushConstantRange.stageFlags =
            HgiVkConversions::GetShaderStages(pcDesc.stageUsage);

        pcRanges.emplace_back(std::move(pushConstantRange));
    }

    VkPipelineLayoutCreateInfo pipeLayCreateInfo =
        {VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
    pipeLayCreateInfo.pushConstantRangeCount = (uint32_t) pcRanges.size();
    pipeLayCreateInfo.pPushConstantRanges = pcRanges.data();
    pipeLayCreateInfo.setLayoutCount = 1;
    pipeLayCreateInfo.pSetLayouts = &_vkDescriptorSetLayout;

    TF_VERIFY(
        vkCreatePipelineLayout(
            _device->GetVulkanDevice(),
            &pipeLayCreateInfo,
            HgiVkAllocator(),
            &_vkPipelineLayout) == VK_SUCCESS
    );
}

HgiVkResourceBindings::~HgiVkResourceBindings()
{
    vkDestroyDescriptorSetLayout(
        _device->GetVulkanDevice(),
        _vkDescriptorSetLayout,
        HgiVkAllocator());

    vkDestroyPipelineLayout(
        _device->GetVulkanDevice(),
        _vkPipelineLayout,
        HgiVkAllocator());

    // Since we have one pool for this resourceBindings we can reset the pool
    // instead of freeing the descriptorSet.
    //
    // if (_vkDescriptorSet) {
    //     vkFreeDescriptorSets(
    //         _device->GetVulkanDevice(),
    //         _vkDescriptorPool,
    //         1,
    //         &_vkDescriptorSet);
    // }
    //
    vkDestroyDescriptorPool(
        _device->GetVulkanDevice(),
        _vkDescriptorPool,
        HgiVkAllocator());
}

HgiBufferBindDescVector const&
HgiVkResourceBindings::GetBufferBindings() const
{
    return _descriptor.buffers;
}

HgiTextureBindDescVector const&
HgiVkResourceBindings::GetTextureBindings() const
{
    return _descriptor.textures;
}

HgiVertexBufferDescVector const&
HgiVkResourceBindings::GetVertexBuffers() const
{
    return _descriptor.vertexBuffers;
}

void
HgiVkResourceBindings::BindResources(HgiVkCommandBuffer* cb)
{
    VkPipelineBindPoint bindPoint =
        _descriptor.pipelineType == HgiPipelineTypeCompute ?
        VK_PIPELINE_BIND_POINT_COMPUTE :
        VK_PIPELINE_BIND_POINT_GRAPHICS;

    // When binding new resources for the currently bound pipeline it may
    // 'disturb' previously bound resources (for a previous pipeline) that
    // are no longer compatible with the layout for the new pipeline.
    // This essentially unbinds the old resources.

    vkCmdBindDescriptorSets(
        cb->GetCommandBufferForRecoding(),
        bindPoint,
        _vkPipelineLayout,
        0, // firstSet
        1, // descriptorSetCount -- strict limits, see maxBoundDescriptorSets
        &_vkDescriptorSet,
        0, // dynamicOffset
        nullptr);
}

VkPipelineLayout
HgiVkResourceBindings::GetPipelineLayout() const
{
    return _vkPipelineLayout;
}

VkDescriptorSet
HgiVkResourceBindings::GetDescriptorSet() const
{
    return _vkDescriptorSet;
}

VkDescriptorImageInfoVector const&
HgiVkResourceBindings::GetImageInfos() const
{
    return _imageInfos;
}

VkDescriptorBufferInfoVector const&
HgiVkResourceBindings::GetBufferInfos() const
{
    return _bufferInfos;
}

PXR_NAMESPACE_CLOSE_SCOPE
