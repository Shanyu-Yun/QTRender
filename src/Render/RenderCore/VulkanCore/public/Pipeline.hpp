/**
 * @file Pipeline.hpp
 * @author Summer
 * @brief Vulkan 图形管线的 RAII 封装和构建器。
 *
 * 该文件提供了现代化的 Vulkan 管线创建接口，专为 Vulkan 1.3+ 动态渲染设计。
 * 主要特性：
 * - 仅支持动态渲染（不支持传统 RenderPass）
 * - 链式调用的构建器模式
 * - RAII 资源管理
 * - 线程安全的管线绑定
 *
 * @note 动态渲染的优势：
 *   - 无需预先创建 RenderPass 和 Framebuffer
 *   - 更灵活的渲染通道配置（LoadOp/StoreOp 在运行时指定）
 *   - 同一管线可用于不同的渲染配置
 *
 * @version 1.0
 * @date 2025-10-29
 */

#pragma once
#include "Device.hpp"
#include "ShaderManager.hpp"

namespace vkcore
{
// 前置声明
class PipelineBuilder;

/**
 * @class Pipeline
 * @brief Vulkan 管线的 RAII 封装，管理 vk::Pipeline 和 vk::PipelineLayout
 */
class Pipeline
{
  public:
    /**
     * @brief 析构函数，自动销毁管线和管线布局
     */
    ~Pipeline();

    /** 禁用拷贝构造与拷贝赋值 */
    Pipeline(const Pipeline &) = delete;
    Pipeline &operator=(const Pipeline &) = delete;

    /** 禁用移动构造与移动赋值（管线是重量级对象，推荐使用智能指针）*/
    Pipeline(Pipeline &&) = delete;
    Pipeline &operator=(Pipeline &&) = delete;

    /**
     * @brief 获取 Vulkan 管线句柄
     * @return vk::Pipeline
     */
    vk::Pipeline get() const
    {
        return m_pipeline;
    }

    /**
     * @brief 获取 Vulkan 管线布局句柄
     * @return vk::PipelineLayout
     */
    vk::PipelineLayout getLayout() const
    {
        return m_pipelineLayout;
    }

    /**
     * @brief 将管线绑定到命令缓冲区
     * @param cmd 要绑定的命令缓冲区
     */
    void bind(vk::CommandBuffer cmd);

  private:
    /**
     * @brief 私有构造函数，仅限 PipelineBuilder 调用
     */
    Pipeline(vkcore::Device &device, vk::Pipeline pipeline, vk::PipelineLayout layout, vk::PipelineBindPoint bindPoint);

  private:
    vkcore::Device &m_device;            ///< 逻辑设备引用
    vk::Pipeline m_pipeline;             ///< Vulkan 管线句柄
    vk::PipelineLayout m_pipelineLayout; ///< Vulkan 管线布局句柄
    vk::PipelineBindPoint m_bindPoint;   ///< 管线绑定点 (图形或计算)

    // 允许 PipelineBuilder 访问私有构造函数
    friend class PipelineBuilder;
};

/**
 * @class PipelineBuilder
 * @brief Vulkan 图形管线构建器（仅支持动态渲染）
 *
 * 该构建器专为 Vulkan 1.3+ 动态渲染设计，不支持传统的 RenderPass 方式。
 * 必须至少指定一个颜色、深度或模板附件格式才能构建管线。
 *
 * @note 动态渲染的 LoadOp/StoreOp 在录制命令时通过 vkCmdBeginRendering 配置，
 *       而不是在管线创建时配置。这使得同一个管线可以用于不同的渲染通道。
 */
class PipelineBuilder
{
  public:
    /**
     * @brief 构造函数
     * @param device 逻辑设备引用
     */
    PipelineBuilder(vkcore::Device &device);
    ~PipelineBuilder() = default;

    /** 禁用拷贝与移动 */
    PipelineBuilder(const PipelineBuilder &) = delete;
    PipelineBuilder &operator=(const PipelineBuilder &) = delete;

    // ==================== 管线配置 (链式调用) ====================

    /**
     * @brief 添加一个着色器阶段
     * @param shaderModule 从 ShaderManager 获取的共享指针
     * @return PipelineBuilder& 自身引用
     */
    PipelineBuilder &addShaderModule(std::shared_ptr<ShaderModule> shaderModule);

    /**
     * @brief 添加一个描述符集布局
     * @param layout 描述符集布局句柄 (例如由 DescriptorLayoutBuilder 创建)
     * @return PipelineBuilder& 自身引用
     */
    PipelineBuilder &addDescriptorSetLayout(vk::DescriptorSetLayout layout);

    /**
     * @brief 添加一个推送常量范围
     * @param range 推送常量范围
     * @return PipelineBuilder& 自身引用
     */
    PipelineBuilder &addPushConstant(const vk::PushConstantRange &range);

    /**
     * @brief 设置顶点输入状态
     * @param info 顶点输入状态创建信息
     * @return PipelineBuilder& 自身引用
     */
    PipelineBuilder &setVertexInput(const vk::PipelineVertexInputStateCreateInfo &info);

    /**
     * @brief 设置输入装配状态 (拓扑结构)
     * @param info 输入装配状态创建信息
     * @return PipelineBuilder& 自身引用
     */
    PipelineBuilder &setInputAssembly(const vk::PipelineInputAssemblyStateCreateInfo &info);

    /**
     * @brief 设置光栅化状态
     * @param info 光栅化状态创建信息
     * @return PipelineBuilder& 自身引用
     */
    PipelineBuilder &setRasterization(const vk::PipelineRasterizationStateCreateInfo &info);

    /**
     * @brief 设置多重采样状态
     * @param info 多重采样状态创建信息
     * @return PipelineBuilder& 自身引用
     */
    PipelineBuilder &setMultisampling(const vk::PipelineMultisampleStateCreateInfo &info);

    /**
     * @brief 设置深度/模板状态
     * @param info 深度/模板状态创建信息
     * @return PipelineBuilder& 自身引用
     */
    PipelineBuilder &setDepthStencil(const vk::PipelineDepthStencilStateCreateInfo &info);

    /**
     * @brief 添加一个颜色附件 (用于动态渲染)
     * @param format 附件的格式 (例如 SwapChain 格式)
     * @param blendState 该附件的颜色混合状态
     * @return PipelineBuilder& 自身引用
     * @note 动态渲染中，管线只需知道附件格式。LoadOp/StoreOp 在 vkCmdBeginRendering 时配置。
     */
    PipelineBuilder &addColorAttachment(vk::Format format, const vk::PipelineColorBlendAttachmentState &blendState);

    /**
     * @brief 设置深度附件格式 (用于动态渲染)
     * @param format 深度附件的格式 (例如 eD32Sfloat)
     * @return PipelineBuilder& 自身引用
     * @note 深度附件的 LoadOp/StoreOp 也在 vkCmdBeginRendering 时配置
     */
    PipelineBuilder &setDepthAttachment(vk::Format format);

    /**
     * @brief 设置模板附件格式 (用于动态渲染)
     * @param format 模板附件的格式 (例如 eS8Uint)
     * @return PipelineBuilder& 自身引用
     */
    PipelineBuilder &setStencilAttachment(vk::Format format);

    /**
     * @brief 添加一个动态状态 (例如 Viewport, Scissor)
     * @param state 要动态设置的状态
     * @return PipelineBuilder& 自身引用
     */
    PipelineBuilder &addDynamicState(vk::DynamicState state);

    // ==================== 构建 ====================

    /**
     * @brief 构建图形管线（动态渲染）
     *
     * 创建支持 Vulkan 1.3 动态渲染的图形管线。必须至少指定一个附件格式。
     *
     * @return std::unique_ptr<Pipeline> 返回一个 RAII 封装的 Pipeline 对象
     * @throws std::runtime_error 如果未指定任何附件格式（不支持传统 RenderPass）
     * @throws std::runtime_error 如果管线创建失败
     *
     * @note 不支持传统的 RenderPass 渲染方式，必须使用动态渲染
     */
    std::unique_ptr<Pipeline> build();

  private:
    /**
     * @brief 内部函数：创建管线布局
     * @return vk::PipelineLayout 创建的布局句柄
     */
    vk::PipelineLayout buildlayout();

  private:
    vkcore::Device &m_device;

    // --- 管线配置 ---
    std::vector<std::shared_ptr<ShaderModule>> m_shaderModules;
    std::vector<vk::DescriptorSetLayout> m_setLayouts;
    std::vector<vk::PushConstantRange> m_pushConstants;

    vk::PipelineVertexInputStateCreateInfo m_vertexInputInfo;
    vk::PipelineInputAssemblyStateCreateInfo m_inputAssemblyInfo;
    vk::PipelineRasterizationStateCreateInfo m_rasterizationInfo;
    vk::PipelineMultisampleStateCreateInfo m_multisampleInfo;
    vk::PipelineDepthStencilStateCreateInfo m_depthStencilInfo;
    std::vector<vk::PipelineColorBlendAttachmentState> m_colorBlendAttachments;
    std::vector<vk::DynamicState> m_dynamicStates;

    // --- 动态渲染 (Vulkan 1.3) ---
    std::vector<vk::Format> m_colorAttachmentFormats;
    vk::Format m_depthAttachmentFormat = vk::Format::eUndefined;
    vk::Format m_stencilAttachmentFormat = vk::Format::eUndefined;
};

} // namespace vkcore
