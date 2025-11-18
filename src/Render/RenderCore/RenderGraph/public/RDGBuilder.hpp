/**
 * @file RDGBuilder.hpp
 * @brief 公开的图构建器 - 用户的主要接口
 * @details 每帧用于声明渲染图的前端接口，负责记录渲染意图
 */

#pragma once

#include "RDGHandle.hpp"
#include "RDGPass.hpp"
#include "RDGSyncInfo.hpp"
#include <memory>

// 前向声明
namespace vkcore
{
class Device;
class CommandPoolManager;
class SwapChain;
class Image;
class Buffer;
} // namespace vkcore

typedef struct VmaAllocator_T *VmaAllocator;

namespace rendercore
{

// 前向声明内部实现
class RenderGraph;

/**
 * @class RDGBuilder
 * @brief 每帧用于声明渲染图的前端接口
 * @details 使用声明式API构建渲染图，由内部RenderGraph类处理编译和执行
 */
class RDGBuilder
{
  public:
    /**
     * @brief 构造函数
     * @param device Vulkan设备
     * @param cmdManager 命令池管理器
     * @param allocator VMA分配器
     */
    RDGBuilder(vkcore::Device &device, vkcore::CommandPoolManager &cmdManager, VmaAllocator allocator);

    /**
     * @brief 析构函数
     * @details 如果未显式调用execute()，析构函数中会自动执行
     */
    ~RDGBuilder();

    // 禁用拷贝和移动
    RDGBuilder(const RDGBuilder &) = delete;
    RDGBuilder &operator=(const RDGBuilder &) = delete;
    RDGBuilder(RDGBuilder &&) = delete;
    RDGBuilder &operator=(RDGBuilder &&) = delete;

    // ==================== Pass管理 ====================

    /**
     * @brief 添加一个渲染通道（简单版本）
     * @param name 通道的调试名称
     * @param callback 包含Vulkan命令录制的Lambda函数
     * @return RDGPass& 通道对象的引用，用于链式声明依赖
     *
     * @example
     * @code
     * builder.addPass("ShadowPass", [](vk::CommandBuffer cmd) {
     *     // 录制阴影渲染命令
     * }).writeDepthStencilAttachment(shadowMap)
     *   .readBuffer(sceneUBO);
     * @endcode
     */
    RDGPass &addPass(std::string name, RDGPass::ExecuteCallback &&callback);

    /**
     * @brief 添加一个渲染通道（扩展版本，可访问资源）
     * @param name 通道的调试名称
     * @param callback 包含Vulkan命令录制的Lambda函数，可访问资源
     * @return RDGPass& 通道对象的引用，用于链式声明依赖
     *
     * @example 读取临时纹理
     * @code
     * auto gbuffer = builder.createColorBuffer("GBuffer", 1920, 1080);
     *
     * builder.addPass("LightingPass", [gbuffer](vk::CommandBuffer cmd, RDGResourceAccessor& res) {
     *     // 获取 G-Buffer 的 ImageView 和采样器
     *     vk::ImageView view = res.getTextureView(gbuffer);
     *     vk::Sampler sampler = res.getSampler(RDGSamplerType::NearestClamp);
     *
     *     // 创建描述符集并绑定...
     *     // 录制光照计算命令...
     * }).readTexture(gbuffer);
     * @endcode
     *
     * @example 混合使用外部纹理和临时纹理
     * @code
     * auto externalTex = resourceManager.getTexture("albedo.png");
     * auto albedoHandle = builder.registerExternalTexture(externalTex->image.get(), "Albedo");
     * auto tempTarget = builder.createColorBuffer("TempRT", 512, 512);
     *
     * builder.addPass("BlendPass", [externalTex](vk::CommandBuffer cmd, RDGResourceAccessor& res) {
     *     // 外部纹理：使用它自己的 Sampler
     *     vk::ImageView albedoView = res.getTextureView(albedoHandle);
     *     vk::Sampler albedoSampler = externalTex->sampler.get();
     *
     *     // 临时纹理：使用通用 Sampler
     *     vk::ImageView tempView = res.getTextureView(tempTarget);
     *     vk::Sampler tempSampler = res.getDefaultSampler();
     *
     *     // 绑定并渲染...
     * }).readTexture(albedoHandle)
     *   .readTexture(tempTarget);
     * @endcode
     */
    RDGPass &addPass(std::string name, RDGPass::ExecuteCallbackEx &&callback);

    // ==================== 瞬态资源创建 ====================

    /**
     * @brief 创建一个瞬态纹理（例如G-Buffer、深度图）
     * @param desc 纹理描述
     * @return RDGTextureHandle 虚拟句柄
     *
     * @details 瞬态资源只在当前帧存在，会被自动管理生命周期和内存复用
     */
    RDGTextureHandle createTexture(const RDGTextureDesc &desc);

    /**
     * @brief 创建一个瞬态缓冲区
     * @param desc 缓冲区描述
     * @return RDGBufferHandle 虚拟句柄
     */
    RDGBufferHandle createBuffer(const RDGBufferDesc &desc);

    // ==================== 外部资源导入 ====================

    /**
     * @brief 导入一个外部（持久化）纹理到图中
     * @param image 指向由ResourceManager或SwapChain管理的vkcore::Image
     * @param name 调试名称
     * @param currentLayout 当前图像布局
     * @return RDGTextureHandle 虚拟句柄
     *
     * @details 外部资源不会被自动释放，需要外部管理生命周期
     */
    RDGTextureHandle registerExternalTexture(vkcore::Image *image, const std::string &name = "ExternalTexture",
                                             vk::ImageLayout currentLayout = vk::ImageLayout::eUndefined);

    /**
     * @brief 导入一个外部（持久化）缓冲区到图中
     * @param buffer 指向由ResourceManager管理的vkcore::Buffer
     * @param name 调试名称
     * @return RDGBufferHandle 虚拟句柄
     */
    RDGBufferHandle registerExternalBuffer(vkcore::Buffer *buffer, const std::string &name = "ExternalBuffer");

    // ==================== 便利函数 ====================

    /**
     * @brief 辅助函数：快速导入交换链后台缓冲区
     * @param swapChain 交换链
     * @param imageIndex 当前图像索引
     * @return RDGTextureHandle 虚拟句柄
     */
    RDGTextureHandle getSwapChainAttachment(vkcore::SwapChain &swapChain, uint32_t imageIndex);

    /**
     * @brief 创建2D纹理的便利函数
     * @param name 纹理名称
     * @param format 纹理格式
     * @param width 宽度
     * @param height 高度
     * @param usage 使用标志
     * @return RDGTextureHandle 虚拟句柄
     */
    RDGTextureHandle createTexture2D(const std::string &name, vk::Format format, uint32_t width, uint32_t height,
                                     vk::ImageUsageFlags usage);

    /**
     * @brief 创建深度缓冲区的便利函数
     * @param name 纹理名称
     * @param width 宽度
     * @param height 高度
     * @param format 深度格式（默认为D32_SFLOAT）
     * @return RDGTextureHandle 虚拟句柄
     */
    RDGTextureHandle createDepthBuffer(const std::string &name, uint32_t width, uint32_t height,
                                       vk::Format format = vk::Format::eD32Sfloat);

    /**
     * @brief 创建颜色缓冲区的便利函数
     * @param name 纹理名称
     * @param width 宽度
     * @param height 高度
     * @param format 颜色格式（默认为R8G8B8A8_UNORM）
     * @return RDGTextureHandle 虚拟句柄
     */
    RDGTextureHandle createColorBuffer(const std::string &name, uint32_t width, uint32_t height,
                                       vk::Format format = vk::Format::eR8G8B8A8Unorm);

    // ==================== 执行控制 ====================

    /**
     * @brief 编译图，执行所有优化，并提交到GPU
     * @param syncInfo 同步信息（可选），用于正确的 CPU-GPU 和 GPU-GPU 同步
     * @details 执行步骤：
     *          1. 编译：构建依赖图、剔除Pass、解析资源生命周期
     *          2. 分配：瞬态资源分配和内存复用
     *          3. 执行：拓扑排序、自动屏障插入、Pass执行
     *
     * @note 调用此函数后，Builder将变为不可用状态
     * @note 如果提供了 syncInfo，将使用信号量和 Fence 进行同步，不会阻塞 CPU
     *       如果不提供 syncInfo（默认），将使用简单的异步提交（不推荐用于生产环境）
     *
     * @example 使用 SwapChain 同步
     * @code
     * RDGSyncInfo syncInfo;
     * syncInfo.addWaitSemaphore(imageAvailableSemaphore);
     * syncInfo.addSignalSemaphore(renderFinishedSemaphore);
     * syncInfo.setExecutionFence(fence);
     * builder.execute(&syncInfo);
     * @endcode
     */
    void execute(RDGSyncInfo *syncInfo = nullptr);

    /**
     * @brief 检查是否已执行
     */
    bool isExecuted() const
    {
        return m_executed;
    }

    // ==================== 调试和统计 ====================

    /**
     * @brief 获取Pass数量
     */
    size_t getPassCount() const;

    /**
     * @brief 获取瞬态资源数量
     */
    size_t getTransientResourceCount() const;

    /**
     * @brief 设置调试名称（用于调试工具）
     */
    void setDebugName(const std::string &name);

  private:
    // ==================== 内部实现 ====================

    /**
     * @brief 指向PImpl (Pointer to implementation)
     * @details 复杂的编译逻辑被隐藏在RenderGraph类中
     */
    std::unique_ptr<RenderGraph> m_pimpl;

    /**
     * @brief 执行状态标志
     */
    bool m_executed = false;

    /**
     * @brief 验证Builder状态
     * @throws std::runtime_error 如果已执行
     */
    void validateState() const;
};

} // namespace rendercore