/**
 * @file ShaderManager.cpp
 * @author Summer
 * @brief 着色器管理器的实现文件。
 *
 * 该文件包含 ShaderManager 类的成员函数实现，负责着色器模块的加载、缓存和生命周期管理。
 * 主要功能包括：
 * - 基于名称的着色器缓存机制，避免重复加载
 * - 从 SPIR-V 字节码创建着色器模块
 * - 线程安全的并发访问控制
 *
 * 注意：使用 std::shared_ptr 管理着色器模块，支持外部持有引用。
 * @version 1.0
 * @date 2025-10-18
 */

#include "ShaderManager.hpp"

namespace vkcore
{
// ========================================
// ShaderManager 类的实现
// ========================================

ShaderManager::ShaderManager(Device &device) : m_device(device)
{
}

ShaderManager::~ShaderManager()
{
    cleanup();
}

std::shared_ptr<ShaderModule> ShaderManager::getShaderModule(const std::string &name)
{
    std::lock_guard<std::mutex> lock(m_mutex);

    // 查找缓存中的着色器模块
    auto it = m_shaderModules.find(name);
    if (it != m_shaderModules.end())
    {
        return it->second;
    }

    // 未找到，返回空指针
    return nullptr;
}

std::shared_ptr<ShaderModule> ShaderManager::createShaderModule(const std::string &name,
                                                                const std::vector<uint32_t> &code,
                                                                vk::ShaderStageFlagBits stage)
{
    std::lock_guard<std::mutex> lock(m_mutex);

    // 检查缓存中是否已存在该着色器模块
    auto it = m_shaderModules.find(name);
    if (it != m_shaderModules.end())
    {
        // 缓存命中，直接返回已有模块
        return it->second;
    }

    // 缓存未命中，创建新的着色器模块
    vk::ShaderModule shaderModule = createshadermodule(code);
    if (!shaderModule)
    {
        throw std::runtime_error("Failed to create shader module for " + name);
    }

    // 封装为 ShaderModule 对象并加入缓存
    auto shaderModulePtr = std::make_shared<ShaderModule>(m_device, shaderModule, stage);
    m_shaderModules[name] = shaderModulePtr;

    return shaderModulePtr;
}

void ShaderManager::cleanup()
{
    std::lock_guard<std::mutex> lock(m_mutex);

    // 清空缓存，shared_ptr 会在引用计数归零时自动销毁 ShaderModule
    m_shaderModules.clear();
}

vk::ShaderModule ShaderManager::createshadermodule(const std::vector<uint32_t> &code)
{
    vk::ShaderModuleCreateInfo createInfo = {};
    createInfo.codeSize = code.size() * sizeof(uint32_t);
    createInfo.pCode = code.data();

    vk::ShaderModule shaderModule;
    try
    {
        shaderModule = m_device.get().createShaderModule(createInfo);
    }
    catch (const std::exception &e)
    {
        throw std::runtime_error("Failed to create shader module: " + std::string(e.what()));
    }

    return shaderModule;
}
} // namespace vkcore