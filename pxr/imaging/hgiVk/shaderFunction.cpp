#include "pxr/base/tf/diagnostic.h"

#include "pxr/imaging/hgiVk/conversions.h"
#include "pxr/imaging/hgiVk/device.h"
#include "pxr/imaging/hgiVk/shaderFunction.h"

PXR_NAMESPACE_OPEN_SCOPE


HgiVkShaderFunction::HgiVkShaderFunction(
    HgiVkDevice* device,
    HgiShaderFunctionDesc const& desc)
    : HgiShaderFunction(desc)
    , _device(device)
    , _descriptor(desc)
    , _vkShaderModule(nullptr)
{
    VkShaderModuleCreateInfo shaderCreateInfo =
        {VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};

    HgiVkShaderCompiler* shaderCompiler = device->GetShaderCompiler();

    std::vector<unsigned int> spirv;

    // Compile shader and capture errors
    bool result = shaderCompiler->CompileGLSL(
        "no_name_provided_for_shader",
        desc.shaderCode.c_str(),
        1,
        desc.shaderStage,
        &spirv,
        &_errors);

    // Create vulkan module if there were no errors.
    if (result) {
        size_t spirvByteSize = spirv.size() * sizeof(unsigned int);

        shaderCreateInfo.codeSize = spirvByteSize;
        shaderCreateInfo.pCode = (uint32_t*) spirv.data();

        TF_VERIFY(
            vkCreateShaderModule(
                _device->GetVulkanDevice(),
                &shaderCreateInfo,
                HgiVkAllocator(),
                &_vkShaderModule) == VK_SUCCESS
        );
    }
}

HgiVkShaderFunction::~HgiVkShaderFunction()
{
    if (_vkShaderModule) {
        vkDestroyShaderModule(
            _device->GetVulkanDevice(),
            _vkShaderModule,
            HgiVkAllocator());
    }
}

VkShaderStageFlagBits
HgiVkShaderFunction::GetShaderStage() const
{
    return VkShaderStageFlagBits(
        HgiVkConversions::GetShaderStages(_descriptor.shaderStage));
}

VkShaderModule
HgiVkShaderFunction::GetShaderModule() const
{
    return _vkShaderModule;
}

const char*
HgiVkShaderFunction::GetShaderFunctionName() const
{
    static const std::string entry("main");
    return entry.c_str();
}

bool
HgiVkShaderFunction::IsValid() const
{
    return _errors.empty();
}

std::string const&
HgiVkShaderFunction::GetCompileErrors()
{
    return _errors;
}

PXR_NAMESPACE_CLOSE_SCOPE
