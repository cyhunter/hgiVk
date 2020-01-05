#include <vector>

#include "pxr/base/tf/diagnostic.h"

#include "pxr/imaging/hgiVk/commandBuffer.h"
#include "pxr/imaging/hgiVk/conversions.h"
#include "pxr/imaging/hgiVk/device.h"
#include "pxr/imaging/hgiVk/pipeline.h"
#include "pxr/imaging/hgiVk/renderPass.h"
#include "pxr/imaging/hgiVk/resourceBindings.h"
#include "pxr/imaging/hgiVk/shaderProgram.h"
#include "pxr/imaging/hgiVk/shaderFunction.h"

HgiVkPipeline::HgiVkPipeline(
    HgiVkDevice* device,
    HgiPipelineDesc const& desc)
    : HgiPipeline(desc)
    , _device(device)
    , _descriptor(desc)
    , _vkTopology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST)
{
    // We cannot create the vulkan pipeline here, because we need to know the
    // render pass that will be used in combination with this pipeline.
    // We postpone creating the pipeline until BindPipeline(), which must be
    // called after an encoder (render pass) has been activated.
}

HgiVkPipeline::~HgiVkPipeline()
{
    for (_Pipeline p : _pipelines) {
        vkDestroyPipeline(
            _device->GetVulkanDevice(),
            p.vkPipeline,
            HgiVkAllocator());
    }
}

void
HgiVkPipeline::BindPipeline(
    HgiVkCommandBuffer* cb,
    HgiVkRenderPass* rp)
{
    // See constructor. Pipeline creation was delayed until now, because for
    // Vulkan we need to know the render pass to create the pipeline.
    VkPipeline vkPipeline = AcquirePipeline(rp);

    VkPipelineBindPoint bindPoint =
        _descriptor.pipelineType == HgiPipelineTypeCompute ?
        VK_PIPELINE_BIND_POINT_COMPUTE :
        VK_PIPELINE_BIND_POINT_GRAPHICS;

    vkCmdBindPipeline(cb->GetCommandBufferForRecoding(), bindPoint, vkPipeline);
}

VkPipeline
HgiVkPipeline::AcquirePipeline(HgiVkRenderPass* rp)
{
    TF_VERIFY(rp, "RenderPass null when acquiring pipeline.");

    // We don't want clients to have to worry about pipeline - render pass
    // compatibility in Hgi. Clients manage pipelines independently and bind
    // pipelines to encoders. It is therefor possible they may not create an
    // unique pipeline for each encoder.
    // To facilitate that we create vulkan pipelines on-demand here when we
    // receive an incompatible graphics encoder (aka render pass).
    // For more info see vulkan docs: renderpass-compatibility.

    HgiGraphicsEncoderDesc const& desc = rp->GetDescriptor();
    for (_Pipeline p : _pipelines) {
        // XXX it may be beneficial to add a hash onto HgiGraphicsEncoderDesc
        // for cheaper comparing.
        if (p.desc == desc) return p.vkPipeline;
    }

    // todo Below we assume this is a gfx pipeline.
    // We need to also implement compute version: vkCreateComputePipelines
    TF_VERIFY(_descriptor.pipelineType==HgiPipelineTypeGraphics);

    VkGraphicsPipelineCreateInfo pipeCreateInfo =
        {VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO};

    //
    // Shaders
    //
    HgiVkShaderProgram const* shaderProgram =
        static_cast<HgiVkShaderProgram const*>(_descriptor.shaderProgram);
    HgiShaderFunctionHandleVector const& sfv =
        shaderProgram->GetShaderFunctions();

    std::vector<VkPipelineShaderStageCreateInfo> stages;
    stages.reserve(sfv.size());

    for (HgiShaderFunctionHandle const& sf : sfv) {
        HgiVkShaderFunction const* s =
            static_cast<HgiVkShaderFunction const*>(sf);

        VkPipelineShaderStageCreateInfo stage =
            {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO};
        stage.stage = s->GetShaderStage();
        stage.module = s->GetShaderModule();
        stage.pName = s->GetShaderFunctionName();
        stage.pNext = nullptr;
        stage.pSpecializationInfo = nullptr; // XXX allows shader optimizations
        stage.flags = 0;
        stages.emplace_back(std::move(stage));
    }

    pipeCreateInfo.stageCount = (uint32_t) stages.size();
    pipeCreateInfo.pStages = stages.data();

    //
    // Vertex Input State
    // The input state includes the format and arrangement of the vertex data.
    //

    HgiVkResourceBindings* resources =
        static_cast<HgiVkResourceBindings*>(_descriptor.resourceBindings);

    HgiVertexBufferDescVector const& vbos = resources->GetVertexBuffers();
    std::vector<VkVertexInputBindingDescription> vertBufs;
    std::vector<VkVertexInputAttributeDescription> vertAttrs;

    for (HgiVertexBufferDesc const& vbo : vbos) {

        HgiVertexAttributeDescVector const& vas = vbo.vertexAttributes;

        for (size_t loc=0; loc<vas.size(); loc++) {
            HgiVertexAttributeDesc const& va = vas[loc];

            VkVertexInputAttributeDescription ad;
            ad.binding = vbo.bindingIndex;
            ad.location = va.shaderBindLocation;
            ad.offset = va.offset;
            ad.format = HgiVkConversions::GetFormat(va.format);
            vertAttrs.emplace_back(std::move(ad));
        }

        VkVertexInputBindingDescription vib;
        vib.binding = vbo.bindingIndex;
        vib.stride = vbo.vertexStride;
        vib.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
        vertBufs.emplace_back(std::move(vib));
    }

    VkPipelineVertexInputStateCreateInfo vertexInput =
        {VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};
    vertexInput.pVertexAttributeDescriptions = vertAttrs.data();
    vertexInput.vertexAttributeDescriptionCount = (uint32_t) vertAttrs.size();
    vertexInput.pVertexBindingDescriptions = vertBufs.data();
    vertexInput.vertexBindingDescriptionCount = (uint32_t) vertBufs.size();
    pipeCreateInfo.pVertexInputState = &vertexInput;

    //
    // Input assembly state
    // Declare how your vertices form the geometry you want to draw.
    //
    VkPipelineInputAssemblyStateCreateInfo inputAssembly =
        {VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO};
    inputAssembly.topology = _vkTopology;
    pipeCreateInfo.pInputAssemblyState = &inputAssembly;

    //
    // Pipeline layout
    // This was generated when the resource bindings was created.
    //
    pipeCreateInfo.layout = resources->GetPipelineLayout();

    //
    // Viewport and Scissor state
    // If these are set via a command, state this in Dynamic states below.
    //
    VkPipelineViewportStateCreateInfo viewportState =
        {VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO};
    viewportState.viewportCount = 1;
    viewportState.scissorCount = 1;
    viewportState.pScissors = nullptr;
    viewportState.pViewports = nullptr;
    pipeCreateInfo.pViewportState = &viewportState;

    //
    // Rasterization state
    // Rasterization operations.
    //
    HgiRasterizationState const& ras = _descriptor.rasterizationState;

    VkPipelineRasterizationStateCreateInfo rasterState =
        {VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO};
    rasterState.lineWidth = ras.lineWidth;
    rasterState.cullMode = HgiVkConversions::GetCullMode(ras.cullMode);
    rasterState.polygonMode = HgiVkConversions::GetPolygonMode(ras.polygonMode);
    rasterState.frontFace = HgiVkConversions::GetWinding(ras.winding);
    pipeCreateInfo.pRasterizationState = &rasterState;

    //
    // Multisample state
    //
    HgiMultiSampleState const& ms = _descriptor.multiSampleState;

    VkPipelineMultisampleStateCreateInfo multisampleState =
        {VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO};
    multisampleState.pSampleMask = nullptr;
    multisampleState.rasterizationSamples =
        HgiVkConversions::GetSampleCount(ms.rasterizationSamples);
    multisampleState.sampleShadingEnable = ms.sampleShadingEnable;
    multisampleState.alphaToCoverageEnable = ms.alphaToCoverageEnable;
    multisampleState.alphaToOneEnable = VK_FALSE;
    multisampleState.minSampleShading = ms.samplesPerFragment;
    pipeCreateInfo.pMultisampleState = &multisampleState;

    //
    // Depth Stencil state
    //
    VkBool32 depthTest =
        bool(_descriptor.depthState & HgiDepthStateBitsDepthTest);

    VkBool32 depthWrite =
        bool(_descriptor.depthState & HgiBlendStateBitsDepthWrite);

    VkPipelineDepthStencilStateCreateInfo depthStencilState =
        {VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO};

    depthStencilState.depthTestEnable = depthTest;
    depthStencilState.depthWriteEnable = depthWrite;
    depthStencilState.depthCompareOp =
        HgiVkConversions::GetCompareOp(_descriptor.depthCompareOp);
    depthStencilState.depthBoundsTestEnable = VK_FALSE;
    depthStencilState.minDepthBounds = 0;
    depthStencilState.maxDepthBounds = 0;
// todo expose stencil options in hgi
    depthStencilState.stencilTestEnable = VK_FALSE;
    depthStencilState.back.failOp = VK_STENCIL_OP_KEEP;
    depthStencilState.back.passOp = VK_STENCIL_OP_KEEP;
    depthStencilState.back.compareOp = VK_COMPARE_OP_ALWAYS;
    depthStencilState.back.compareMask = 0;
    depthStencilState.back.reference = 0;
    depthStencilState.back.depthFailOp = VK_STENCIL_OP_KEEP;
    depthStencilState.back.writeMask = 0;
    depthStencilState.front = depthStencilState.back;
    pipeCreateInfo.pDepthStencilState = &depthStencilState;

    //
    // Color blend state
    // Per attachment configuration of how output color blends with destination.
    //
    std::vector<VkPipelineColorBlendAttachmentState> colorAttachState;
    for (HgiAttachmentDesc const& attachment : desc.colorAttachments) {
        VkPipelineColorBlendAttachmentState ca =
            {VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO};

// todo get color mask from hgi
        ca.colorWriteMask = VK_COLOR_COMPONENT_R_BIT |
                            VK_COLOR_COMPONENT_G_BIT |
                            VK_COLOR_COMPONENT_B_BIT |
                            VK_COLOR_COMPONENT_A_BIT;

// todo get blend mode settings from hgi
        ca.blendEnable = VK_FALSE;
        ca.alphaBlendOp = VK_BLEND_OP_ADD;
        ca.colorBlendOp = VK_BLEND_OP_ADD;
        ca.srcColorBlendFactor = VK_BLEND_FACTOR_ZERO;
        ca.dstColorBlendFactor = VK_BLEND_FACTOR_ZERO;
        ca.srcAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
        ca.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;

        colorAttachState.emplace_back(std::move(ca));
    }

    VkPipelineColorBlendStateCreateInfo colorBlendState =
        {VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO};
    colorBlendState.attachmentCount = colorAttachState.size();
    colorBlendState.pAttachments = colorAttachState.data();
    colorBlendState.logicOpEnable = VK_FALSE;
    colorBlendState.logicOp = VK_LOGIC_OP_NO_OP;
    colorBlendState.blendConstants[0] = 1.0f;
    colorBlendState.blendConstants[1] = 1.0f;
    colorBlendState.blendConstants[2] = 1.0f;
    colorBlendState.blendConstants[3] = 1.0f;
    pipeCreateInfo.pColorBlendState = &colorBlendState;

    //
    // Dynamic States
    // States that change during command buffer execution via a command
    //
    VkDynamicState dynamicStates[] = {VK_DYNAMIC_STATE_VIEWPORT,
                                      VK_DYNAMIC_STATE_SCISSOR};

    VkPipelineDynamicStateCreateInfo dynamicState =
        {VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO};
    dynamicState.dynamicStateCount = HgiVkArraySize(dynamicStates);
    dynamicState.pDynamicStates = dynamicStates;
    pipeCreateInfo.pDynamicState = &dynamicState;

    //
    // Render pass
    //
    HgiVkRenderPass* renderPass = _device->AcquireRenderPass(desc);
    pipeCreateInfo.renderPass = renderPass->GetVulkanRenderPass();

    //
    // Make pipeline
    //
    _Pipeline pipeline;
    pipeline.desc = desc;

    // xxx we need to add a pipeline cache to avoid app having to keep compiling
    // shader micro-code for every pipeline combination. We except that the
    // spir-V shader code is not compiled for the target device until this point
    // where we create the pipeline. So a pipeline cache can be helpful.
    // https://zeux.io/2019/07/17/serializing-pipeline-cache/
    TF_VERIFY(
        vkCreateGraphicsPipelines(
            _device->GetVulkanDevice(),
            _device->GetVulkanPipelineCache(),
            1,
            &pipeCreateInfo,
            HgiVkAllocator(),
            &pipeline.vkPipeline) == VK_SUCCESS
    );

    _pipelines.push_back(pipeline);
    return pipeline.vkPipeline;
}
