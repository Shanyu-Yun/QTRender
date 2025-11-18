/**
 * @file Pipeline.cpp
 * @author Summer
 * @brief Vulkan 管线的实现文件。
 *
 * 该文件包含 Pipeline 和 PipelineBuilder 类的实现，负责图形管线的创建和管理。
 * 支持 Vulkan 1.3 动态渲染特性，无需预先创建 RenderPass。
 *
 * 注意：动态渲染中，LoadOp/StoreOp 在 vkCmdBeginRendering 时配置，不是在管线创建时。
 * @version 1.0
 * @date 2025-10-29
 */

#include "Pipeline.hpp"
#include <stdexcept>

namespace vkcore
{
// ========================================
// Pipeline 类的实现
// ========================================

Pipeline::Pipeline(vkcore::Device &device, vk::Pipeline pipeline, vk::PipelineLayout layout,
                   vk::PipelineBindPoint bindPoint)
    : m_device(device), m_pipeline(pipeline), m_pipelineLayout(layout), m_bindPoint(bindPoint)
{
}

Pipeline::~Pipeline()
{
    if (m_pipeline)
    {
        m_device.get().destroyPipeline(m_pipeline);
        m_pipeline = nullptr;
    }

    if (m_pipelineLayout)
    {
        m_device.get().destroyPipelineLayout(m_pipelineLayout);
        m_pipelineLayout = nullptr;
    }
}

void Pipeline::bind(vk::CommandBuffer cmd)
{
    cmd.bindPipeline(m_bindPoint, m_pipeline);
}

// ========================================
// PipelineBuilder 类的实现
// ========================================

PipelineBuilder::PipelineBuilder(vkcore::Device &device) : m_device(device)
{
    // 设置默认的顶点输入状态（无顶点输入）
    m_vertexInputInfo.vertexBindingDescriptionCount = 0;
    m_vertexInputInfo.pVertexBindingDescriptions = nullptr;
    m_vertexInputInfo.vertexAttributeDescriptionCount = 0;
    m_vertexInputInfo.pVertexAttributeDescriptions = nullptr;

    // 设置默认的输入装配状态（三角形列表）
    m_inputAssemblyInfo.topology = vk::PrimitiveTopology::eTriangleList;
    m_inputAssemblyInfo.primitiveRestartEnable = VK_FALSE;

    // 设置默认的光栅化状态
    m_rasterizationInfo.depthClampEnable = VK_FALSE;
    m_rasterizationInfo.rasterizerDiscardEnable = VK_FALSE;
    m_rasterizationInfo.polygonMode = vk::PolygonMode::eFill;
    m_rasterizationInfo.cullMode = vk::CullModeFlagBits::eBack;
    m_rasterizationInfo.frontFace = vk::FrontFace::eCounterClockwise;
    m_rasterizationInfo.depthBiasEnable = VK_FALSE;
    m_rasterizationInfo.lineWidth = 1.0f;

    // 设置默认的多重采样状态（禁用）
    m_multisampleInfo.rasterizationSamples = vk::SampleCountFlagBits::e1;
    m_multisampleInfo.sampleShadingEnable = VK_FALSE;
    m_multisampleInfo.minSampleShading = 1.0f;
    m_multisampleInfo.pSampleMask = nullptr;
    m_multisampleInfo.alphaToCoverageEnable = VK_FALSE;
    m_multisampleInfo.alphaToOneEnable = VK_FALSE;

    // 设置默认的深度/模板状态（启用深度测试）
    m_depthStencilInfo.depthTestEnable = VK_TRUE;
    m_depthStencilInfo.depthWriteEnable = VK_TRUE;
    m_depthStencilInfo.depthCompareOp = vk::CompareOp::eLess;
    m_depthStencilInfo.depthBoundsTestEnable = VK_FALSE;
    m_depthStencilInfo.stencilTestEnable = VK_FALSE;
}

PipelineBuilder &PipelineBuilder::addShaderModule(std::shared_ptr<ShaderModule> shaderModule)
{
    if (shaderModule)
    {
        m_shaderModules.push_back(shaderModule);
    }
    return *this;
}

PipelineBuilder &PipelineBuilder::addDescriptorSetLayout(vk::DescriptorSetLayout layout)
{
    m_setLayouts.push_back(layout);
    return *this;
}

PipelineBuilder &PipelineBuilder::addPushConstant(const vk::PushConstantRange &range)
{
    m_pushConstants.push_back(range);
    return *this;
}

PipelineBuilder &PipelineBuilder::setVertexInput(const vk::PipelineVertexInputStateCreateInfo &info)
{
    m_vertexInputInfo = info;
    return *this;
}

PipelineBuilder &PipelineBuilder::setInputAssembly(const vk::PipelineInputAssemblyStateCreateInfo &info)
{
    m_inputAssemblyInfo = info;
    return *this;
}

PipelineBuilder &PipelineBuilder::setRasterization(const vk::PipelineRasterizationStateCreateInfo &info)
{
    m_rasterizationInfo = info;
    return *this;
}

PipelineBuilder &PipelineBuilder::setMultisampling(const vk::PipelineMultisampleStateCreateInfo &info)
{
    m_multisampleInfo = info;
    return *this;
}

PipelineBuilder &PipelineBuilder::setDepthStencil(const vk::PipelineDepthStencilStateCreateInfo &info)
{
    m_depthStencilInfo = info;
    return *this;
}

PipelineBuilder &PipelineBuilder::addColorAttachment(vk::Format format,
                                                     const vk::PipelineColorBlendAttachmentState &blendState)
{
    m_colorAttachmentFormats.push_back(format);
    m_colorBlendAttachments.push_back(blendState);
    return *this;
}

PipelineBuilder &PipelineBuilder::setDepthAttachment(vk::Format format)
{
    m_depthAttachmentFormat = format;
    return *this;
}

PipelineBuilder &PipelineBuilder::setStencilAttachment(vk::Format format)
{
    m_stencilAttachmentFormat = format;
    return *this;
}

PipelineBuilder &PipelineBuilder::addDynamicState(vk::DynamicState state)
{
    m_dynamicStates.push_back(state);
    return *this;
}

vk::PipelineLayout PipelineBuilder::buildlayout()
{
    vk::PipelineLayoutCreateInfo layoutInfo = {};
    layoutInfo.setLayoutCount = static_cast<uint32_t>(m_setLayouts.size());
    layoutInfo.pSetLayouts = m_setLayouts.data();
    layoutInfo.pushConstantRangeCount = static_cast<uint32_t>(m_pushConstants.size());
    layoutInfo.pPushConstantRanges = m_pushConstants.data();

    vk::PipelineLayout layout;
    try
    {
        layout = m_device.get().createPipelineLayout(layoutInfo);
    }
    catch (const std::exception &e)
    {
        throw std::runtime_error(std::string("Failed to create pipeline layout: ") + e.what());
    }

    return layout;
}

std::unique_ptr<Pipeline> PipelineBuilder::build()
{
    // 0. 验证必须使用动态渲染
    if (m_colorAttachmentFormats.empty() && m_depthAttachmentFormat == vk::Format::eUndefined &&
        m_stencilAttachmentFormat == vk::Format::eUndefined)
    {
        throw std::runtime_error("Dynamic rendering is required! Must specify at least one attachment format using "
                                 "addColorAttachment(), setDepthAttachment(), or setStencilAttachment()");
    }

    // 1. 创建管线布局
    vk::PipelineLayout layout = buildlayout();

    // 2. 准备着色器阶段
    std::vector<vk::PipelineShaderStageCreateInfo> shaderStages;
    for (const auto &shader : m_shaderModules)
    {
        vk::PipelineShaderStageCreateInfo stageInfo = {};
        stageInfo.stage = shader->stage;
        stageInfo.module = shader->shaderModule;
        stageInfo.pName = "main"; // 入口函数名
        shaderStages.push_back(stageInfo);
    }

    if (shaderStages.empty())
    {
        throw std::runtime_error("No shader modules provided to PipelineBuilder");
    }

    // 3. 配置视口和裁剪矩形（如果使用动态状态，这里可以设置为 nullptr）
    vk::PipelineViewportStateCreateInfo viewportState = {};
    viewportState.viewportCount = 1;
    viewportState.scissorCount = 1;

    // 4. 配置颜色混合状态
    vk::PipelineColorBlendStateCreateInfo colorBlendState = {};
    colorBlendState.logicOpEnable = VK_FALSE;
    colorBlendState.logicOp = vk::LogicOp::eCopy;
    colorBlendState.attachmentCount = static_cast<uint32_t>(m_colorBlendAttachments.size());
    colorBlendState.pAttachments = m_colorBlendAttachments.data();
    colorBlendState.blendConstants[0] = 0.0f;
    colorBlendState.blendConstants[1] = 0.0f;
    colorBlendState.blendConstants[2] = 0.0f;
    colorBlendState.blendConstants[3] = 0.0f;

    // 5. 配置动态状态
    vk::PipelineDynamicStateCreateInfo dynamicState = {};
    dynamicState.dynamicStateCount = static_cast<uint32_t>(m_dynamicStates.size());
    dynamicState.pDynamicStates = m_dynamicStates.data();

    // 6. 配置动态渲染信息 (Vulkan 1.3)
    vk::PipelineRenderingCreateInfo renderingInfo = {};
    renderingInfo.colorAttachmentCount = static_cast<uint32_t>(m_colorAttachmentFormats.size());
    renderingInfo.pColorAttachmentFormats = m_colorAttachmentFormats.data();
    renderingInfo.depthAttachmentFormat = m_depthAttachmentFormat;
    renderingInfo.stencilAttachmentFormat = m_stencilAttachmentFormat;

    // 7. 创建图形管线
    vk::GraphicsPipelineCreateInfo pipelineInfo = {};
    pipelineInfo.pNext = &renderingInfo; // 关键：链接动态渲染信息
    pipelineInfo.stageCount = static_cast<uint32_t>(shaderStages.size());
    pipelineInfo.pStages = shaderStages.data();
    pipelineInfo.pVertexInputState = &m_vertexInputInfo;
    pipelineInfo.pInputAssemblyState = &m_inputAssemblyInfo;
    pipelineInfo.pViewportState = &viewportState;
    pipelineInfo.pRasterizationState = &m_rasterizationInfo;
    pipelineInfo.pMultisampleState = &m_multisampleInfo;
    pipelineInfo.pDepthStencilState = &m_depthStencilInfo;
    pipelineInfo.pColorBlendState = &colorBlendState;
    pipelineInfo.pDynamicState = m_dynamicStates.empty() ? nullptr : &dynamicState;
    pipelineInfo.layout = layout;
    pipelineInfo.renderPass = nullptr; // 动态渲染不需要 RenderPass
    pipelineInfo.subpass = 0;
    pipelineInfo.basePipelineHandle = nullptr;
    pipelineInfo.basePipelineIndex = -1;

    vk::Pipeline pipeline;
    try
    {
        // createGraphicsPipelines 返回 ResultValue<std::vector<Pipeline>>
        vk::ResultValue<std::vector<vk::Pipeline>> result =
            m_device.get().createGraphicsPipelines(nullptr, pipelineInfo);

        if (result.result != vk::Result::eSuccess)
        {
            throw std::runtime_error("Failed to create graphics pipeline");
        }
        pipeline = result.value[0];
    }
    catch (const std::exception &e)
    {
        // 创建失败时清理已创建的布局
        m_device.get().destroyPipelineLayout(layout);
        throw std::runtime_error(std::string("Failed to create graphics pipeline: ") + e.what());
    }

    // 8. 返回封装后的 Pipeline 对象
    return std::unique_ptr<Pipeline>(new Pipeline(m_device, pipeline, layout, vk::PipelineBindPoint::eGraphics));
}

} // namespace vkcore
