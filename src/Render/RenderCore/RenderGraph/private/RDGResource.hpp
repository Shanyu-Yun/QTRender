/**
 * @file RDGResource.hpp
 * @brief 内部资源表示 - RenderGraph内部使用的资源管理
 * @details 定义内部资源表示和生命周期管理
 */

#pragma once

#include "RDGHandle.hpp"
#include "VulkanCore/public/VKResource.hpp"
#include <memory>
#include <string>
#include <vector>

namespace rendercore
{

/**
 * @enum RDGResourceType
 * @brief 内部资源类型
 */
enum class RDGResourceType : uint8_t
{
    Transient, ///< 瞬态资源（当前帧创建和销毁）
    External   ///< 外部资源（持久化，外部管理）
};

/**
 * @enum RDGResourceState
 * @brief 资源状态
 */
enum class RDGResourceState : uint8_t
{
    Declared,  ///< 已声明，未分配
    Allocated, ///< 已分配物理资源
    Active,    ///< 正在使用中
    Finished   ///< 使用完毕，可回收
};

/**
 * @struct RDGResourceLifetime
 * @brief 资源生命周期信息
 */
struct RDGResourceLifetime
{
    uint32_t firstPassIndex = UINT32_MAX; ///< 第一次使用的Pass索引
    uint32_t lastPassIndex = 0;           ///< 最后一次使用的Pass索引
    bool isUsed = false;                  ///< 是否被使用

    /**
     * @brief 检查是否与另一个生命周期重叠
     */
    bool overlapsWith(const RDGResourceLifetime &other) const
    {
        if (!isUsed || !other.isUsed)
            return false;
        return !(lastPassIndex < other.firstPassIndex || other.lastPassIndex < firstPassIndex);
    }

    /**
     * @brief 更新生命周期范围
     */
    void updateUsage(uint32_t passIndex)
    {
        if (!isUsed)
        {
            firstPassIndex = passIndex;
            lastPassIndex = passIndex;
            isUsed = true;
        }
        else
        {
            firstPassIndex = std::min(firstPassIndex, passIndex);
            lastPassIndex = std::max(lastPassIndex, passIndex);
        }
    }
};

// ==================== 纹理资源 ====================

/**
 * @class RDGTextureResource
 * @brief 内部纹理资源表示
 */
class RDGTextureResource
{
  public:
    RDGTextureResource(RDGResourceHandle handle, const RDGTextureDesc &desc,
                       RDGResourceType type = RDGResourceType::Transient);

    RDGTextureResource(RDGResourceHandle handle, vkcore::Image *externalImage, const std::string &name,
                       vk::ImageLayout currentLayout);

    ~RDGTextureResource() = default;

    // 访问器
    RDGResourceHandle getHandle() const
    {
        return m_handle;
    }
    const std::string &getName() const
    {
        return m_desc.name;
    }
    const RDGTextureDesc &getDesc() const
    {
        return m_desc;
    }
    RDGResourceType getType() const
    {
        return m_type;
    }
    RDGResourceState getState() const
    {
        return m_state;
    }
    const RDGResourceLifetime &getLifetime() const
    {
        return m_lifetime;
    }

    // 物理资源
    vkcore::Image *getPhysicalImage() const
    {
        return m_physicalImage;
    }
    void setPhysicalImage(vkcore::Image *image)
    {
        m_physicalImage = image;
        m_state = RDGResourceState::Allocated;
    }

    // 布局管理
    vk::ImageLayout getCurrentLayout() const
    {
        return m_currentLayout;
    }
    void setCurrentLayout(vk::ImageLayout layout)
    {
        m_currentLayout = layout;
    }

    // 状态管理
    void setState(RDGResourceState state)
    {
        m_state = state;
    }

    // 生命周期管理
    void updateLifetime(uint32_t passIndex)
    {
        m_lifetime.updateUsage(passIndex);
    }
    bool isUsed() const
    {
        return m_lifetime.isUsed;
    }
    bool isTransient() const
    {
        return m_type == RDGResourceType::Transient;
    }
    bool isExternal() const
    {
        return m_type == RDGResourceType::External;
    }

    /**
     * @brief 检查是否可以与另一个资源复用物理内存
     */
    bool canAliasWith(const RDGTextureResource &other) const;

    // SwapChain 支持
    bool isSwapChainImage() const
    {
        return m_swapChainImageIndex != UINT32_MAX;
    }
    uint32_t getSwapChainImageIndex() const
    {
        return m_swapChainImageIndex;
    }
    void setSwapChainImageIndex(uint32_t index)
    {
        m_swapChainImageIndex = index;
    }

  private:
    RDGResourceHandle m_handle;
    RDGTextureDesc m_desc;
    RDGResourceType m_type;
    RDGResourceState m_state = RDGResourceState::Declared;
    RDGResourceLifetime m_lifetime;

    // 物理资源
    vkcore::Image *m_physicalImage = nullptr;
    vk::ImageLayout m_currentLayout = vk::ImageLayout::eUndefined;

    // SwapChain 支持
    uint32_t m_swapChainImageIndex = UINT32_MAX; ///< SwapChain图像索引，UINT32_MAX表示不是SwapChain图像
};

// ==================== 缓冲区资源 ====================

/**
 * @class RDGBufferResource
 * @brief 内部缓冲区资源表示
 */
class RDGBufferResource
{
  public:
    RDGBufferResource(RDGResourceHandle handle, const RDGBufferDesc &desc,
                      RDGResourceType type = RDGResourceType::Transient);

    RDGBufferResource(RDGResourceHandle handle, vkcore::Buffer *externalBuffer, const std::string &name);

    ~RDGBufferResource() = default;

    // 访问器
    RDGResourceHandle getHandle() const
    {
        return m_handle;
    }
    const std::string &getName() const
    {
        return m_desc.name;
    }
    const RDGBufferDesc &getDesc() const
    {
        return m_desc;
    }
    RDGResourceType getType() const
    {
        return m_type;
    }
    RDGResourceState getState() const
    {
        return m_state;
    }
    const RDGResourceLifetime &getLifetime() const
    {
        return m_lifetime;
    }

    // 物理资源
    vkcore::Buffer *getPhysicalBuffer() const
    {
        return m_physicalBuffer;
    }
    void setPhysicalBuffer(vkcore::Buffer *buffer)
    {
        m_physicalBuffer = buffer;
        m_state = RDGResourceState::Allocated;
    }

    // 状态管理
    void setState(RDGResourceState state)
    {
        m_state = state;
    }

    // 生命周期管理
    void updateLifetime(uint32_t passIndex)
    {
        m_lifetime.updateUsage(passIndex);
    }
    bool isUsed() const
    {
        return m_lifetime.isUsed;
    }
    bool isTransient() const
    {
        return m_type == RDGResourceType::Transient;
    }
    bool isExternal() const
    {
        return m_type == RDGResourceType::External;
    }

    /**
     * @brief 检查是否可以与另一个资源复用物理内存
     */
    bool canAliasWith(const RDGBufferResource &other) const;

  private:
    RDGResourceHandle m_handle;
    RDGBufferDesc m_desc;
    RDGResourceType m_type;
    RDGResourceState m_state = RDGResourceState::Declared;
    RDGResourceLifetime m_lifetime;

    // 物理资源
    vkcore::Buffer *m_physicalBuffer = nullptr;
};

// ==================== 资源池 ====================

/**
 * @class RDGResourcePool
 * @brief 瞬态资源池，用于内存复用
 */
template <typename ResourceType> class RDGResourcePool
{
  public:
    /**
     * @brief 尝试获取一个可复用的资源
     * @param requiredDesc 所需资源的描述符（用于匹配资源规格）
     * @param lifetime 资源生命周期（用于检查是否可以复用）
     */
    template <typename DescType>
    ResourceType *tryAcquire(const DescType &requiredDesc, const RDGResourceLifetime &lifetime)
    {
        (void)lifetime; // 当前简化实现中未使用，保留用于未来的生命周期重叠检查

        // 查找一个规格匹配的可用资源
        for (auto it = m_availableResources.begin(); it != m_availableResources.end(); ++it)
        {
            ResourceType *resource = it->get();

            // 检查资源规格是否匹配
            if (isCompatible(resource, requiredDesc))
            {
                // 找到匹配的资源，从池中移除并返回
                ResourceType *result = resource;
                m_availableResources.erase(it);
                return result;
            }
        }

        return nullptr; // 没有找到可复用的资源
    }

    /**
     * @brief 释放资源到池中
     */
    void release(std::unique_ptr<ResourceType> resource)
    {
        if (resource)
        {
            m_availableResources.push_back(std::move(resource));
        }
    }

    /**
     * @brief 清空池
     */
    void clear()
    {
        m_availableResources.clear();
    }

    /**
     * @brief 获取池中资源数量
     */
    size_t size() const
    {
        return m_availableResources.size();
    }

  private:
    /**
     * @brief 检查vkcore::Image是否与RDGTextureDesc兼容
     */
    bool isCompatible(vkcore::Image *image, const RDGTextureDesc &desc) const
    {
        // 注意：不检查 samples，因为 vkcore::Image 当前没有 getSamples() 方法
        return image->getFormat() == desc.format && image->getExtent() == desc.extent &&
               image->getUsage() == desc.usage && image->getMipLevels() == desc.mipLevels &&
               image->getArrayLayers() == desc.arrayLayers;
    }

    /**
     * @brief 检查vkcore::Buffer是否与RDGBufferDesc兼容
     */
    bool isCompatible(vkcore::Buffer *buffer, const RDGBufferDesc &desc) const
    {
        return buffer->getSize() >= desc.size && // 允许更大的缓冲区
               buffer->getUsage() == desc.usage;
    }

    std::vector<std::unique_ptr<ResourceType>> m_availableResources;
};

using RDGTexturePool = RDGResourcePool<vkcore::Image>;
using RDGBufferPool = RDGResourcePool<vkcore::Buffer>;

} // namespace rendercore