/**
 * @file RenderGraph.hpp
 * @brief RDG后端编译器 - 处理图编译、优化和执行
 * @details 内部实现类，负责所有复杂的编译逻辑、优化和执行
 */

#pragma once

#include "../public/RDGHandle.hpp"
#include "../public/RDGPass.hpp"
#include "../public/RDGResourceAccessor.hpp"
#include "../public/RDGSyncInfo.hpp"
#include "RDGResource.hpp"
#include <array>
#include <memory>
#include <unordered_map>
#include <vector>


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

/**
 * @struct RDGBarrier
 * @brief 屏障信息
 */
struct RDGBarrier
{
    enum Type
    {
        Image,
        Buffer
    } type;
    RDGResourceHandle handle;
    vk::PipelineStageFlags srcStages;
    vk::PipelineStageFlags dstStages;
    vk::AccessFlags srcAccess;
    vk::AccessFlags dstAccess;

    // 仅对图像有效
    vk::ImageLayout oldLayout = vk::ImageLayout::eUndefined;
    vk::ImageLayout newLayout = vk::ImageLayout::eUndefined;
    vk::ImageSubresourceRange subresourceRange;
};

/**
 * @class RDGCompiledPass
 * @brief 编译后的Pass信息
 */
class RDGCompiledPass
{
  public:
    RDGCompiledPass(const RDGPass &pass, uint32_t index) : m_originalPass(&pass), m_index(index), m_active(true)
    {
    }

    const RDGPass *getOriginalPass() const
    {
        return m_originalPass;
    }
    uint32_t getIndex() const
    {
        return m_index;
    }
    bool isActive() const
    {
        return m_active;
    }
    void setActive(bool active)
    {
        m_active = active;
    }

    const std::vector<RDGBarrier> &getBarriers() const
    {
        return m_barriers;
    }
    void addBarrier(const RDGBarrier &barrier)
    {
        m_barriers.push_back(barrier);
    }

    bool isGraphicsPass() const
    {
        return m_originalPass->isGraphicsPass();
    }
    bool isComputePass() const
    {
        return m_originalPass->isComputePass();
    }

  private:
    const RDGPass *m_originalPass;
    uint32_t m_index;
    bool m_active;
    std::vector<RDGBarrier> m_barriers; ///< 此Pass执行前需要的屏障
};

/**
 * @class RenderGraph
 * @brief RDG后端编译器和执行器
 * @details PImpl实现，隐藏复杂的编译和优化逻辑
 */
class RenderGraph
{
  public:
    /**
     * @brief 构造函数
     */
    RenderGraph(vkcore::Device &device, vkcore::CommandPoolManager &cmdManager, VmaAllocator allocator);

    /**
     * @brief 析构函数
     */
    ~RenderGraph();

    // 禁用拷贝和移动
    RenderGraph(const RenderGraph &) = delete;
    RenderGraph &operator=(const RenderGraph &) = delete;

    // ==================== 资源管理接口 ====================

    /**
     * @brief 创建瞬态纹理资源
     */
    RDGTextureHandle createTransientTexture(const RDGTextureDesc &desc);

    /**
     * @brief 创建瞬态缓冲区资源
     */
    RDGBufferHandle createTransientBuffer(const RDGBufferDesc &desc);

    /**
     * @brief 注册外部纹理资源
     */
    RDGTextureHandle registerExternalTexture(vkcore::Image *image, const std::string &name,
                                             vk::ImageLayout currentLayout);

    /**
     * @brief 注册外部缓冲区资源
     */
    RDGBufferHandle registerExternalBuffer(vkcore::Buffer *buffer, const std::string &name);

    /**
     * @brief 导入交换链图像
     */
    RDGTextureHandle importSwapChainImage(vkcore::SwapChain &swapChain, uint32_t imageIndex);

    // ==================== Pass管理接口 ====================

    /**
     * @brief 添加渲染Pass（简单版本）
     */
    RDGPass &addPass(std::string name, RDGPass::ExecuteCallback &&callback);

    /**
     * @brief 添加渲染Pass（扩展版本，可访问资源）
     */
    RDGPass &addPassEx(std::string name, RDGPass::ExecuteCallbackEx &&callback);

    // ==================== 编译和执行接口 ====================

    /**
     * @brief 编译渲染图
     * @details 执行依赖分析、Pass剔除、资源生命周期分析
     */
    void compile();

    /**
     * @brief 执行渲染图（带同步信息）
     * @param syncInfo 同步信息，包含需要等待的信号量和执行完成后要触发的信号量/Fence
     * @details 分配资源、插入屏障、执行所有活跃Pass
     * @note 不再使用 waitIdle()，而是通过 syncInfo 进行正确的同步
     */
    void execute(RDGSyncInfo *syncInfo = nullptr);

    // ==================== 查询接口 ====================

    /**
     * @brief 获取Pass数量
     */
    size_t getPassCount() const
    {
        return m_passes.size();
    }

    /**
     * @brief 获取瞬态资源数量
     */
    size_t getTransientResourceCount() const;

    /**
     * @brief 设置调试名称
     */
    void setDebugName(const std::string &name)
    {
        m_debugName = name;
    }

    // ==================== 调试接口 ====================

    /**
     * @brief 获取编译后的Pass信息（用于调试）
     */
    const std::vector<std::unique_ptr<RDGCompiledPass>> &getCompiledPasses() const
    {
        return m_compiledPasses;
    }

    // ==================== 资源访问接口（供 RDGResourceAccessor 使用）====================

    /**
     * @brief 获取纹理的物理资源
     * @note 仅在 Pass 执行期间有效
     */
    vkcore::Image *getPhysicalTexture(RDGTextureHandle handle) const;

    /**
     * @brief 获取缓冲区的物理资源
     * @note 仅在 Pass 执行期间有效
     */
    vkcore::Buffer *getPhysicalBuffer(RDGBufferHandle handle) const;

    /**
     * @brief 获取纹理的当前布局
     */
    vk::ImageLayout getTextureLayout(RDGTextureHandle handle) const;

    /**
     * @brief 获取预定义的采样器
     */
    vk::Sampler getSampler(RDGSamplerType type) const;

  private:
    // ==================== 内部编译阶段 ====================

    /**
     * @brief 阶段1：构建依赖图
     */
    void buildDependencyGraph();

    /**
     * @brief 阶段2：剔除无用Pass
     */
    void cullUnusedPasses();

    /**
     * @brief 阶段3：分析资源生命周期
     */
    void analyzeResourceLifetime();

    /**
     * @brief 阶段4：分配物理资源
     */
    void allocateResources();

    /**
     * @brief 阶段5：计算屏障
     */
    void computeBarriers();

    // ==================== 资源分配辅助函数 ====================

    /**
     * @brief 分配瞬态纹理
     */
    vkcore::Image *allocateTransientTexture(RDGTextureResource &resource);

    /**
     * @brief 分配瞬态缓冲区
     */
    vkcore::Buffer *allocateTransientBuffer(RDGBufferResource &resource);

    /**
     * @brief 尝试复用已有资源
     */
    template <typename ResourceType>
    ResourceType *tryReuseResource(const RDGResourceLifetime &lifetime, const typename ResourceType::DescType &desc);

    // ==================== 屏障计算辅助函数 ====================

    /**
     * @brief 计算纹理布局转换
     */
    vk::ImageLayout computeImageLayout(RDGTextureHandle handle, const RDGPass::TextureAccess &access) const;

    /**
     * @brief 添加图像内存屏障
     */
    void addImageBarrier(RDGCompiledPass &pass, RDGTextureHandle handle, vk::ImageLayout oldLayout,
                         vk::ImageLayout newLayout, vk::AccessFlags srcAccess, vk::AccessFlags dstAccess,
                         vk::PipelineStageFlags srcStages, vk::PipelineStageFlags dstStages);

    /**
     * @brief 添加缓冲区内存屏障
     */
    void addBufferBarrier(RDGCompiledPass &pass, RDGBufferHandle handle, vk::AccessFlags srcAccess,
                          vk::AccessFlags dstAccess, vk::PipelineStageFlags srcStages,
                          vk::PipelineStageFlags dstStages);

    // ==================== 执行辅助函数 ====================

    /**
     * @brief 执行屏障
     */
    void executeBarriers(vk::CommandBuffer cmd, const std::vector<RDGBarrier> &barriers);

    /**
     * @brief 开始图形Pass（设置渲染状态）
     */
    void beginGraphicsPass(vk::CommandBuffer cmd, const RDGPass &pass);

    /**
     * @brief 结束图形Pass
     */
    void endGraphicsPass(vk::CommandBuffer cmd);

    // ==================== 验证辅助函数 ====================

    /**
     * @brief 验证资源状态
     */
    void validateResourceStates() const;

    // ==================== 句柄生成 ====================

    /**
     * @brief 生成下一个资源句柄
     */
    RDGResourceHandle generateNextHandle()
    {
        return ++m_nextHandle;
    }

    // ==================== 成员变量 ====================

    // 核心依赖
    vkcore::Device &m_device;
    vkcore::CommandPoolManager &m_commandManager;
    VmaAllocator m_allocator;

    // 句柄生成
    RDGResourceHandle m_nextHandle = 0;

    // Pass存储
    std::vector<std::unique_ptr<RDGPass>> m_passes;
    std::vector<std::unique_ptr<RDGCompiledPass>> m_compiledPasses;

    // 资源存储
    std::unordered_map<RDGResourceHandle, std::unique_ptr<RDGTextureResource>> m_textureResources;
    std::unordered_map<RDGResourceHandle, std::unique_ptr<RDGBufferResource>> m_bufferResources;

    // 资源池（用于内存复用）
    RDGTexturePool m_texturePool;
    RDGBufferPool m_bufferPool;

    // 当前帧分配的资源（执行完毕后释放）
    std::vector<std::unique_ptr<vkcore::Image>> m_frameTextures;
    std::vector<std::unique_ptr<vkcore::Buffer>> m_frameBuffers;

    // 采样器池（用于临时纹理采样）
    std::array<vk::Sampler, static_cast<size_t>(RDGSamplerType::Count)> m_samplers;
    bool m_samplersCreated = false;

    // 资源布局跟踪（用于屏障计算）
    std::unordered_map<RDGResourceHandle, vk::ImageLayout> m_textureLayouts;

    // SwapChain跟踪（用于处理SwapChain图像）
    std::unordered_map<RDGResourceHandle, vkcore::SwapChain *> m_swapChainMapping;

    // 编译状态
    bool m_compiled = false;
    bool m_executed = false;

    // 调试信息
    std::string m_debugName = "RenderGraph";

    // 当前渲染状态
    bool m_insideGraphicsPass = false;

    // ==================== 辅助方法 ====================

    /**
     * @brief 创建采样器池
     */
    void createSamplers();

    /**
     * @brief 销毁采样器池
     */
    void destroySamplers();
};

} // namespace rendercore