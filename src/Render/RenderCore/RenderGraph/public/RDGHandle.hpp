/**
 * @file RDGHandle.hpp
 * @brief 虚拟资源句柄定义 - RDG的核心抽象
 * @details 轻量级ID，用于在声明阶段代表资源，不占用GPU显存
 */

#pragma once

#include <cstdint>
#include <string>
#include <vulkan/vulkan.hpp>

namespace rendercore
{

// ==================== 基础句柄类型 ====================

/**
 * @enum RDGHandleType
 * @brief 资源句柄类型枚举
 */
enum class RDGHandleType : uint8_t
{
    Texture,
    Buffer
};

/**
 * @typedef RDGResourceHandle
 * @brief 通用资源句柄，内部实现为索引
 */
using RDGResourceHandle = uint32_t;
constexpr RDGResourceHandle kInvalidHandle = 0;

// ==================== 类型安全的句柄 ====================

/**
 * @struct RDGTextureHandle
 * @brief 纹理资源句柄（类型安全，防止将Buffer传给需要Texture的函数）
 */
struct RDGTextureHandle
{
    RDGResourceHandle handle = kInvalidHandle;

    bool isValid() const
    {
        return handle != kInvalidHandle;
    }

    friend bool operator==(const RDGTextureHandle &a, const RDGTextureHandle &b)
    {
        return a.handle == b.handle;
    }

    friend bool operator!=(const RDGTextureHandle &a, const RDGTextureHandle &b)
    {
        return !(a == b);
    }
};

/**
 * @struct RDGBufferHandle
 * @brief 缓冲区资源句柄（类型安全）
 */
struct RDGBufferHandle
{
    RDGResourceHandle handle = kInvalidHandle;

    bool isValid() const
    {
        return handle != kInvalidHandle;
    }

    friend bool operator==(const RDGBufferHandle &a, const RDGBufferHandle &b)
    {
        return a.handle == b.handle;
    }

    friend bool operator!=(const RDGBufferHandle &a, const RDGBufferHandle &b)
    {
        return !(a == b);
    }
};

// ==================== 资源描述符 ====================

/**
 * @struct RDGTextureDesc
 * @brief 瞬态纹理资源的描述（用于创建）
 * @details 简化的纹理描述，对应vkcore::ImageDesc
 */
struct RDGTextureDesc
{
    std::string name;                                              ///< 调试名称
    vk::Format format = vk::Format::eUndefined;                    ///< 纹理格式
    vk::Extent3D extent{0, 0, 1};                                  ///< 纹理尺寸
    vk::ImageUsageFlags usage;                                     ///< 使用标志
    uint32_t mipLevels = 1;                                        ///< Mip层级数
    uint32_t arrayLayers = 1;                                      ///< 数组层数
    vk::SampleCountFlagBits samples = vk::SampleCountFlagBits::e1; ///< 多重采样
    vk::ImageTiling tiling = vk::ImageTiling::eOptimal;            ///< 内存排列方式

    /**
     * @brief 构造函数，快速创建2D纹理描述
     */
    RDGTextureDesc(const std::string &name, vk::Format format, uint32_t width, uint32_t height,
                   vk::ImageUsageFlags usage)
        : name(name), format(format), extent{width, height, 1}, usage(usage)
    {
    }

    /**
     * @brief 默认构造函数
     */
    RDGTextureDesc() = default;

    /**
     * @brief 检查描述是否有效
     */
    bool isValid() const
    {
        return format != vk::Format::eUndefined && extent.width > 0 && extent.height > 0 && extent.depth > 0;
    }
};

/**
 * @struct RDGBufferDesc
 * @brief 瞬态缓冲区资源的描述（用于创建）
 * @details 简化的缓冲区描述，对应vkcore::BufferDesc
 */
struct RDGBufferDesc
{
    std::string name;           ///< 调试名称
    vk::DeviceSize size = 0;    ///< 缓冲区大小（字节）
    vk::BufferUsageFlags usage; ///< 使用标志

    /**
     * @brief 构造函数
     */
    RDGBufferDesc(const std::string &name, vk::DeviceSize size, vk::BufferUsageFlags usage)
        : name(name), size(size), usage(usage)
    {
    }

    /**
     * @brief 默认构造函数
     */
    RDGBufferDesc() = default;

    /**
     * @brief 检查描述是否有效
     */
    bool isValid() const
    {
        return size > 0;
    }
};

// ==================== 辅助常量 ====================

/// @brief 无效的纹理句柄常量
constexpr RDGTextureHandle kInvalidTextureHandle{kInvalidHandle};

/// @brief 无效的缓冲区句柄常量
constexpr RDGBufferHandle kInvalidBufferHandle{kInvalidHandle};

} // namespace rendercore
