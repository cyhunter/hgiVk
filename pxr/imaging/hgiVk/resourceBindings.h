#ifndef PXR_IMAGING_HGIVK_RESOURCEBINDINGS_H
#define PXR_IMAGING_HGIVK_RESOURCEBINDINGS_H

#include <vector>

#include "pxr/pxr.h"

#include "pxr/imaging/hgi/resourceBindings.h"
#include "pxr/imaging/hgiVk/api.h"
#include "pxr/imaging/hgiVk/vulkan.h"


PXR_NAMESPACE_OPEN_SCOPE

class HgiVkDevice;
class HgiVkCommandBuffer;

typedef std::vector<VkDescriptorImageInfo> VkDescriptorImageInfoVector;
typedef std::vector<VkDescriptorBufferInfo> VkDescriptorBufferInfoVector;


///
/// \class HgiVkResourceBindings
///
/// Vulkan implementation of HgiResourceBindings.
///
/// There is a limit to how many descriptor sets that can be bound at one time.
/// Aiming for 4 seems like a safe minimum:
/// http://vulkan.gpuinfo.org/displaydevicelimit.php?name=maxBoundDescriptorSets
///
/// This does not affect how many sets you can make, but you likely want to
/// group resources together so you don't have to bind more than ~4 sets.
///
/// You also want to avoid re-creating resourceBindings frequently as our
/// design is to have one descriptor pool per resourceBindings.
///
class HgiVkResourceBindings final : public HgiResourceBindings {
public:
    HGIVK_API
    HgiVkResourceBindings(
        HgiVkDevice* device,
        HgiResourceBindingsDesc const& desc);

    HGIVK_API
    virtual ~HgiVkResourceBindings();

    /// Returns the list of buffers that needs to be bound.
    HGIVK_API
    HgiBufferBindDescVector const& GetBufferBindings() const;

    /// Returns the list of textures that need to be bound.
    HGIVK_API
    HgiTextureBindDescVector const& GetTextureBindings() const;

    /// Returns the list of vertex buffers that describe the vertex attributes.
    HGIVK_API
    HgiVertexBufferDescVector const& GetVertexBuffers() const;

    /// Binds the resources to GPU.
    HGIVK_API
    void BindResources(HgiVkCommandBuffer* cb);

    /// Returns the pipeline layout.
    HGIVK_API
    VkPipelineLayout GetPipelineLayout() const;

    /// Returns the descriptor set
    HGIVK_API
    VkDescriptorSet GetDescriptorSet() const;

    /// Returns the vector of imageInfo's used to make this resourceBindings.
    HGIVK_API
    VkDescriptorImageInfoVector const& GetImageInfos() const;

    /// Returns the vector of bufferInfo's used to make this resourceBindings.
    HGIVK_API
    VkDescriptorBufferInfoVector const& GetBufferInfos() const;

private:
    HgiVkResourceBindings() = delete;
    HgiVkResourceBindings & operator=(const HgiVkResourceBindings&) = delete;
    HgiVkResourceBindings(const HgiVkResourceBindings&) = delete;

private:
    HgiVkDevice* _device;
    HgiResourceBindingsDesc _descriptor;

    VkDescriptorImageInfoVector _imageInfos;
    VkDescriptorBufferInfoVector _bufferInfos;

    VkDescriptorPool _vkDescriptorPool;
    VkDescriptorSetLayout _vkDescriptorSetLayout;
    VkDescriptorSet _vkDescriptorSet;
    VkPipelineLayout _vkPipelineLayout;
};


PXR_NAMESPACE_CLOSE_SCOPE

#endif