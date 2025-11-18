/**
 * @file RDGResourceAccessor.hpp
 * @brief 渲染图资源访问器 - 在 Pass 执行时访问物理资源
 * @details 提供从虚拟句柄到物理资源（ImageView、Buffer、Sampler）的访问
 *
 * @note 资源类型说明：
 * 1. **临时纹理（RDGTextureHandle）**：
 *    - 由 RenderGraph 创建的瞬态资源（createTexture/createColorBuffer/createDepthBuffer）
 *    - 只包含 vkcore::Image（没有 Sampler）
 *    - 主要用于渲染目标（Color/Depth Attachment）
 *    - 如需采样，使用 RenderGraph 提供的通用 Sampler（getSampler）
 *
 * 2. **外部纹理**：
 *    - 由 ResourceManager 创建的持久化 Texture（Image + Sampler）
 *    - 通过 registerExternalTexture 导入到 RenderGraph
 *    - 应该使用 Texture 自己的 Sampler，而非通用 Sampler
 *
 * @example 使用临时纹理
 * @code
 * // 创建临时 G-Buffer
 * auto gbuffer = builder.createColorBuffer("GBuffer", width, height);
 *
 * // 几何 Pass：写入 G-Buffer
 * builder.addPass("GeometryPass", [](vk::CommandBuffer cmd, RDGResourceAccessor& res) {
 *     // 作为渲染目标，不需要 Sampler
 * }).writeColorAttachment(gbuffer);
 *
 * // 光照 Pass：读取 G-Buffer
 * builder.addPass("LightingPass", [](vk::CommandBuffer cmd, RDGResourceAccessor& res) {
 *     vk::ImageView view = res.getTextureView(gbuffer);
 *     vk::Sampler sampler = res.getSampler(RDGSamplerType::NearestClamp); // 使用通用 Sampler
 *     // 创建描述符集，绑定 view + sampler
 * }).readTexture(gbuffer);
 * @endcode
 *
 * @example 使用外部纹理（来自 ResourceManager）
 * @code
 * // 从 ResourceManager 获取持久化纹理
 * auto externalTexture = resourceManager.getTexture("albedo.png");
 *
 * // 导入到 RenderGraph
 * auto textureHandle = builder.registerExternalTexture(
 *     externalTexture->image.get(), "AlbedoMap");
 *
 * // 使用纹理
 * builder.addPass("RenderPass", [externalTexture](vk::CommandBuffer cmd, RDGResourceAccessor& res) {
 *     vk::ImageView view = res.getTextureView(textureHandle);
 *     vk::Sampler sampler = externalTexture->sampler.get(); // 使用 Texture 自己的 Sampler
 *     // 创建描述符集，绑定 view + sampler
 * }).readTexture(textureHandle);
 * @endcode
 */

#pragma once

#include "RDGHandle.hpp"
#include <unordered_map>
#include <vulkan/vulkan.hpp>


// 前向声明
namespace vkcore
{
class Image;
class Buffer;
} // namespace vkcore

namespace rendercore
{

// 前向声明
class RenderGraph;

/**
 * @enum RDGSamplerType
 * @brief 预定义的采样器类型
 */
enum class RDGSamplerType : uint8_t
{
    NearestClamp,      ///< 最近邻，钳位模式
    NearestRepeat,     ///< 最近邻，重复模式
    LinearClamp,       ///< 线性，钳位模式
    LinearRepeat,      ///< 线性，重复模式
    AnisotropicClamp,  ///< 各向异性，钳位模式
    AnisotropicRepeat, ///< 各向异性，重复模式
    ShadowPCF,         ///< 阴影 PCF（Percentage Closer Filtering）
    Count              ///< 采样器数量
};

/**
 * @class RDGResourceAccessor
 * @brief 资源访问器 - 在 Pass 执行时访问物理资源
 * @details 只在 Pass 执行回调中有效，不应该被缓存
 */
class RDGResourceAccessor
{
  public:
    /**
     * @brief 构造函数（仅供 RenderGraph 内部使用）
     */
    RDGResourceAccessor(RenderGraph *renderGraph);

    // ==================== 纹理资源访问 ====================

    /**
     * @brief 获取纹理的 ImageView
     * @param handle 纹理句柄
     * @return vk::ImageView ImageView 句柄，如果资源无效则返回 nullptr
     * @note 用于创建描述符集
     */
    vk::ImageView getTextureView(RDGTextureHandle handle) const;

    /**
     * @brief 获取纹理的底层 Image 对象（高级用法）
     * @param handle 纹理句柄
     * @return vkcore::Image* Image 指针，如果资源无效则返回 nullptr
     * @warning 仅在需要直接访问 Image 属性时使用
     */
    vkcore::Image *getTexture(RDGTextureHandle handle) const;

    /**
     * @brief 获取纹理的当前布局
     * @param handle 纹理句柄
     * @return vk::ImageLayout 当前布局
     */
    vk::ImageLayout getTextureLayout(RDGTextureHandle handle) const;

    // ==================== 缓冲区资源访问 ====================

    /**
     * @brief 获取缓冲区的 Buffer 句柄
     * @param handle 缓冲区句柄
     * @return vk::Buffer Buffer 句柄，如果资源无效则返回 nullptr
     * @note 用于创建描述符集或绑定顶点/索引缓冲区
     */
    vk::Buffer getBuffer(RDGBufferHandle handle) const;

    /**
     * @brief 获取缓冲区的底层 Buffer 对象（高级用法）
     * @param handle 缓冲区句柄
     * @return vkcore::Buffer* Buffer 指针，如果资源无效则返回 nullptr
     */
    vkcore::Buffer *getBufferObject(RDGBufferHandle handle) const;

    /**
     * @brief 获取缓冲区的设备地址（用于光线追踪等）
     * @param handle 缓冲区句柄
     * @return vk::DeviceAddress 设备地址
     */
    vk::DeviceAddress getBufferDeviceAddress(RDGBufferHandle handle) const;

    // ==================== 采样器访问 ====================

    /**
     * @brief 获取预定义的采样器
     * @param type 采样器类型
     * @return vk::Sampler 采样器句柄
     * @note 这些采样器由 RenderGraph 自动创建和管理，主要用于临时纹理
     * @warning 对于外部纹理（从 ResourceManager 导入），应该使用 Texture 自己的 Sampler
     *
     * @example 使用示例（临时纹理）
     * @code
     * // 临时纹理：使用 RenderGraph 的通用 Sampler
     * auto tempTexture = builder.createColorBuffer("Temp", 512, 512);
     * builder.addPass("SamplePass", [](vk::CommandBuffer cmd, RDGResourceAccessor& resources) {
     *     vk::ImageView view = resources.getTextureView(tempTexture);
     *     vk::Sampler sampler = resources.getSampler(RDGSamplerType::LinearClamp);
     *     // 创建描述符集并绑定 view + sampler...
     * }).readTexture(tempTexture);
     * @endcode
     *
     * @example 使用示例（外部纹理）
     * @code
     * // 外部纹理：使用 Texture 自己的 Sampler
     * auto externalTex = resourceManager.getTexture("albedo.png");
     * auto handle = builder.registerExternalTexture(externalTex->image.get(), "Albedo");
     *
     * builder.addPass("RenderPass", [externalTex](vk::CommandBuffer cmd, RDGResourceAccessor& res) {
     *     vk::ImageView view = res.getTextureView(handle);
     *     vk::Sampler sampler = externalTex->sampler.get(); // 使用 Texture 的 Sampler
     *     // 创建描述符集并绑定...
     * }).readTexture(handle);
     * @endcode
     */
    vk::Sampler getSampler(RDGSamplerType type) const;

    /**
     * @brief 获取最常用的线性钳位采样器（便利函数）
     * @return vk::Sampler 采样器句柄
     */
    vk::Sampler getDefaultSampler() const
    {
        return getSampler(RDGSamplerType::LinearClamp);
    }

  private:
    RenderGraph *m_renderGraph; ///< 指向 RenderGraph 实例（不拥有）
};

} // namespace rendercore
