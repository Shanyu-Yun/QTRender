/**
 * @file ShaderManager.hpp
 * @author Summer
 * @brief 着色器管理器及着色器模块封装的头文件。
 *
 * 该文件定义了 Vulkan 着色器系统的核心组件，包括：
 * - ShaderModule：着色器模块的 RAII 封装，自动管理生命周期
 * - ShaderManager：着色器模块的缓存管理器，避免重复加载相同着色器
 *
 * ShaderManager 使用名称作为键缓存已加载的着色器模块，支持线程安全的并发访问。
 * 注意：ShaderModule 禁用拷贝但支持移动语义，ShaderManager 完全禁用拷贝和移动。
 * @version 1.0
 * @date 2025-10-18
 */

#pragma once

#include "Device.hpp"
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vulkan/vulkan.hpp>

namespace vkcore
{
/**
 * @struct ShaderModule
 * @brief 着色器模块的 RAII 封装，负责自动管理 Vulkan 着色器模块的生命周期。
 *
 * 该结构体封装了 vk::ShaderModule 及其相关信息（设备、着色器阶段），
 * 在析构时自动销毁着色器模块资源。支持移动语义但禁用拷贝。
 */
struct ShaderModule
{
    vk::Device device;             ///< Vulkan 逻辑设备句柄
    vk::ShaderModule shaderModule; ///< 着色器模块句柄
    vk::ShaderStageFlagBits stage; ///< 着色器阶段标志（顶点、片段等）

    /**
     * @brief 构造函数，创建着色器模块封装。
     * @param dev 逻辑设备对象引用
     * @param module 着色器模块句柄
     * @param st 着色器阶段标志
     */
    ShaderModule(Device &dev, vk::ShaderModule module, vk::ShaderStageFlagBits st)
        : device(dev.get()), shaderModule(module), stage(st)
    {
    }

    /**
     * @brief 析构函数，自动销毁着色器模块。
     */
    ~ShaderModule()
    {
        if (shaderModule)
        {
            device.destroyShaderModule(shaderModule);
            shaderModule = nullptr;
        }
    }

    // 禁用拷贝，启用移动
    ShaderModule(const ShaderModule &) = delete;
    ShaderModule &operator=(const ShaderModule &) = delete;

    /**
     * @brief 移动构造函数。
     * @param other 被移动的对象
     */
    ShaderModule(ShaderModule &&other) noexcept
        : device(other.device), shaderModule(other.shaderModule), stage(other.stage)
    {
        other.shaderModule = nullptr;
        other.device = nullptr;
    }

    /**
     * @brief 移动赋值运算符。
     * @param other 被移动的对象
     * @return ShaderModule& 自身引用
     */
    ShaderModule &operator=(ShaderModule &&other) noexcept
    {
        if (this != &other)
        {
            if (shaderModule)
            {
                device.destroyShaderModule(shaderModule);
            }
            device = other.device;
            shaderModule = other.shaderModule;
            stage = other.stage;
            other.shaderModule = nullptr;
        }
        return *this;
    }
};

/**
 * @class ShaderManager
 * @brief 着色器模块管理器类，负责着色器的加载、缓存和生命周期管理。
 *
 * 该类使用名称作为键缓存已加载的着色器模块，避免重复加载相同的着色器。
 * 支持两种使用模式：
 * 1. 获取已缓存的着色器模块（仅传入名称）
 * 2. 创建并缓存新的着色器模块（传入名称、代码、阶段）
 *
 * 线程安全：所有公共方法均使用互斥锁保护。
 * 生命周期：禁用拷贝和移动，确保单一所有权。
 */
class ShaderManager
{
  public:
    /**
     * @brief 构造函数。
     * @param device 逻辑设备引用
     */
    ShaderManager(Device &device);

    /**
     * @brief 析构函数，自动调用 cleanup() 清理所有着色器模块。
     */
    ~ShaderManager();

    /** 禁用拷贝和移动 */
    ShaderManager(const ShaderManager &) = delete;
    ShaderManager &operator=(const ShaderManager &) = delete;
    ShaderManager(ShaderManager &&) = delete;
    ShaderManager &operator=(ShaderManager &&) = delete;

    /**
     * @brief 获取已缓存的着色器模块。
     *
     * 根据名称查找已缓存的着色器模块，如果不存在则返回 nullptr。
     * 适用于只需要获取已加载着色器的场景。
     *
     * @param name 着色器名称
     * @return std::shared_ptr<ShaderModule> 着色器模块的智能指针，未找到时为 nullptr
     */
    std::shared_ptr<ShaderModule> getShaderModule(const std::string &name);

    /**
     * @brief 创建或获取着色器模块。
     *
     * 根据名称查找缓存，如果缓存命中则直接返回，否则从 SPIR-V 字节码创建新模块并加入缓存。
     * 适用于首次加载或确保着色器存在的场景。
     *
     * @param name 着色器名称（用于缓存键和调试）
     * @param code SPIR-V 字节码
     * @param stage 着色器阶段标志（顶点、片段、计算等）
     * @return std::shared_ptr<ShaderModule> 着色器模块的智能指针
     * @throws std::runtime_error 如果创建失败
     */
    std::shared_ptr<ShaderModule> createShaderModule(const std::string &name, const std::vector<uint32_t> &code,
                                                     vk::ShaderStageFlagBits stage);

    /**
     * @brief 清理所有缓存的着色器模块。
     *
     * 销毁所有已加载的着色器模块并清空缓存。
     * 注意：由于使用 shared_ptr，只有在所有外部引用释放后才会真正销毁模块。
     */
    void cleanup();

  private:
    /**
     * @brief 内部着色器模块创建函数。
     *
     * 从 SPIR-V 字节码创建 Vulkan 着色器模块，不涉及缓存逻辑。
     *
     * @param code SPIR-V 字节码
     * @return vk::ShaderModule 着色器模块句柄
     * @throws std::runtime_error 如果创建失败
     */
    vk::ShaderModule createshadermodule(const std::vector<uint32_t> &code);

  private:
    /// 逻辑设备引用
    Device &m_device;

    /// 已加载的着色器模块缓存（名称 -> 模块）
    std::unordered_map<std::string, std::shared_ptr<ShaderModule>> m_shaderModules;

    /// 互斥锁，保护线程安全
    mutable std::mutex m_mutex;
};

} // namespace vkcore
