/**
 * @file Descriptor.cpp
 * @author Summer
 * @brief 描述符布局缓存、描述符分配器及相关辅助类的实现文件。
 *
 * 该文件包含 Vulkan 描述符系统的核心实现，包括：
 * - DescriptorLayoutCache：基于哈希表的布局缓存，使用自定义哈希和相等性函数
 * - DescriptorAllocator：池化分配器，支持多种描述符类型和自动扩展
 * - DescriptorLayoutBuilder：构建器模式的简化接口实现
 * - DescriptorUpdater：描述符更新的辅助工具实现
 *
 * 注意：所有涉及多线程访问的方法均使用互斥锁保护。
 * @version 1.0
 * @date 2025-10-18
 */

#include "Descriptor.hpp"

namespace vkcore
{
// ========================================
// DescriptorLayoutCache 类的实现
// ========================================

DescriptorLayoutCache::DescriptorLayoutCache(Device &device) : m_device(device), m_layoutcache()
{
}

DescriptorLayoutCache::~DescriptorLayoutCache()
{
    cleanup();
}

vk::DescriptorSetLayout DescriptorLayoutCache::createDescriptorLayout(
    const std::vector<vk::DescriptorSetLayoutBinding> &bindings)
{
    std::lock_guard<std::mutex> lock(m_mtx);

    // 查找缓存
    auto it = m_layoutcache.find(bindings);
    if (it != m_layoutcache.end())
    {
        // 缓存命中，直接返回已有布局
        return it->second;
    }

    // 缓存未命中，创建新的描述符集布局
    vk::DescriptorSetLayoutCreateInfo layoutInfo = {};
    layoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());
    layoutInfo.pBindings = bindings.data();

    vk::DescriptorSetLayout layout = m_device.get().createDescriptorSetLayout(layoutInfo);

    // 存入正向缓存：绑定列表 -> 布局
    m_layoutcache[bindings] = layout;

    // 存入反向缓存：布局 -> 绑定列表（用于后续查询）
    m_layoutBindings[layout] = bindings;

    return layout;
}

std::vector<vk::DescriptorSetLayoutBinding> DescriptorLayoutCache::getLayoutBindings(
    vk::DescriptorSetLayout layout) const
{
    std::lock_guard<std::mutex> lock(m_mtx);

    auto it = m_layoutBindings.find(layout);
    if (it != m_layoutBindings.end())
    {
        return it->second;
    }

    // 未找到，返回空向量
    return {};
}

void DescriptorLayoutCache::cleanup()
{
    for (const auto &pair : m_layoutcache)
    {
        m_device.get().destroyDescriptorSetLayout(pair.second);
    }
    m_layoutcache.clear();
    m_layoutBindings.clear();
}

std::size_t DescriptorLayoutCache::LayoutInfoHash::operator()(
    const std::vector<vk::DescriptorSetLayoutBinding> &bindings) const
{
    std::size_t hash = bindings.size();

    for (const auto &binding : bindings)
    {
        // 使用哈希组合技术，组合所有关键字段
        // 魔数 0x9e3779b9 是黄金比例的倒数，用于优化哈希分布

        // 组合 binding 索引
        hash ^= std::hash<uint32_t>{}(binding.binding) + 0x9e3779b9 + (hash << 6) + (hash >> 2);

        // 组合描述符类型
        hash ^= std::hash<uint32_t>{}(static_cast<uint32_t>(binding.descriptorType)) + 0x9e3779b9 + (hash << 6) +
                (hash >> 2);

        // 组合描述符数量
        hash ^= std::hash<uint32_t>{}(binding.descriptorCount) + 0x9e3779b9 + (hash << 6) + (hash >> 2);

        // 组合着色器阶段标志
        hash ^=
            std::hash<uint32_t>{}(static_cast<uint32_t>(binding.stageFlags)) + 0x9e3779b9 + (hash << 6) + (hash >> 2);
    }

    return hash;
}

bool DescriptorLayoutCache::LayoutInfoEqual::operator()(const std::vector<vk::DescriptorSetLayoutBinding> &a,
                                                        const std::vector<vk::DescriptorSetLayoutBinding> &b) const
{
    // 首先比较大小
    if (a.size() != b.size())
    {
        return false;
    }

    // 逐个比较每个绑定的关键字段
    for (size_t i = 0; i < a.size(); ++i)
    {
        if (a[i].binding != b[i].binding || a[i].descriptorType != b[i].descriptorType ||
            a[i].descriptorCount != b[i].descriptorCount || a[i].stageFlags != b[i].stageFlags)
        {
            return false;
        }

        // 注意：pImmutableSamplers 通常为 nullptr，如果使用则需要额外比较
        // 这里简化处理，假设不使用不可变采样器或由调用者保证一致性
    }

    return true;
}

// ========================================
// DescriptorAllocator 类的实现
// ========================================

DescriptorAllocator::DescriptorAllocator(Device &device, DescriptorLayoutCache *layoutCache)
    : m_device(device), m_layoutCache(layoutCache)
{
}

DescriptorAllocator::~DescriptorAllocator()
{
    cleanup();
}

void DescriptorAllocator::resetPools()
{
    std::lock_guard<std::mutex> lock(m_mtx);

    // 将已使用的池重置并归还到空闲池列表
    for (auto pool : m_usedPools)
    {
        m_device.get().resetDescriptorPool(pool);
        m_freePools.push_back(pool);
    }
    m_usedPools.clear();
    m_currentPool = nullptr;
}

std::vector<vk::DescriptorSet> DescriptorAllocator::allocate(uint32_t count, vk::DescriptorSetLayout layout)
{
    std::lock_guard<std::mutex> lock(m_mtx);

    // 确保有可用的描述符池
    if (!m_currentPool)
    {
        m_currentPool = grabPool();
    }

    // 准备分配信息（所有描述符集使用相同布局）
    std::vector<vk::DescriptorSetLayout> layouts(count, layout);

    vk::DescriptorSetAllocateInfo allocInfo = {};
    allocInfo.descriptorPool = m_currentPool;
    allocInfo.descriptorSetCount = count;
    allocInfo.pSetLayouts = layouts.data();

    // 尝试从当前池分配描述符集
    try
    {
        auto descriptorSets = m_device.get().allocateDescriptorSets(allocInfo);
        return descriptorSets;
    }
    catch (const vk::OutOfPoolMemoryError &)
    {
        // 当前池已满，获取新池并重试
        m_currentPool = grabPool();
        allocInfo.descriptorPool = m_currentPool;

        try
        {
            return m_device.get().allocateDescriptorSets(allocInfo);
        }
        catch (const vk::SystemError &e)
        {
            throw std::runtime_error(std::string("Failed to allocate descriptor sets: ") + e.what());
        }
    }
    catch (const vk::FragmentedPoolError &)
    {
        // 池碎片化，获取新池并重试
        m_currentPool = grabPool();
        allocInfo.descriptorPool = m_currentPool;

        try
        {
            return m_device.get().allocateDescriptorSets(allocInfo);
        }
        catch (const vk::SystemError &e)
        {
            throw std::runtime_error(std::string("Failed to allocate descriptor sets after pool fragmentation: ") +
                                     e.what());
        }
    }
}

vk::DescriptorSet DescriptorAllocator::allocate(vk::DescriptorSetLayout layout)
{
    std::vector<vk::DescriptorSet> descriptors = allocate(1, layout);
    return descriptors[0];
}

void DescriptorAllocator::cleanup()
{
    std::lock_guard<std::mutex> lock(m_mtx);

    for (auto pool : m_usedPools)
    {
        m_device.get().destroyDescriptorPool(pool);
    }
    m_usedPools.clear();

    for (auto pool : m_freePools)
    {
        m_device.get().destroyDescriptorPool(pool);
    }
    m_freePools.clear();
    m_currentPool = nullptr;
}

vk::DescriptorPool DescriptorAllocator::grabPool()
{
    // 如果有空闲池，直接复用
    if (!m_freePools.empty())
    {
        vk::DescriptorPool pool = m_freePools.back();
        m_freePools.pop_back();
        m_usedPools.push_back(pool);
        return pool;
    }

    // 创建新的描述符池，支持多种常用描述符类型
    // 每种类型分配充足的容量以减少池创建频率
    std::vector<vk::DescriptorPoolSize> poolSizes = {// 组合图像采样器（纹理）- 最常用
                                                     {vk::DescriptorType::eCombinedImageSampler, 1000},

                                                     // Uniform 缓冲区（常量缓冲区）
                                                     {vk::DescriptorType::eUniformBuffer, 1000},

                                                     // Storage 缓冲区（可读写缓冲区，用于计算着色器）
                                                     {vk::DescriptorType::eStorageBuffer, 1000},

                                                     // Dynamic Uniform 缓冲区（动态偏移的常量缓冲区）
                                                     {vk::DescriptorType::eUniformBufferDynamic, 100},

                                                     // Dynamic Storage 缓冲区
                                                     {vk::DescriptorType::eStorageBufferDynamic, 100},

                                                     // 输入附件（用于子通道依赖）
                                                     {vk::DescriptorType::eInputAttachment, 100},

                                                     // 分离的采样器和图像
                                                     {vk::DescriptorType::eSampler, 100},
                                                     {vk::DescriptorType::eSampledImage, 100},

                                                     // Storage 图像（可读写图像，用于计算着色器）
                                                     {vk::DescriptorType::eStorageImage, 100}};

    vk::DescriptorPoolCreateInfo poolInfo = {};
    // 允许单独释放描述符集（更灵活，但性能略低）
    poolInfo.flags = vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet;
    poolInfo.maxSets = 1000; // 池中最多可分配的描述符集总数
    poolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
    poolInfo.pPoolSizes = poolSizes.data();

    vk::DescriptorPool newPool = m_device.get().createDescriptorPool(poolInfo);
    m_usedPools.push_back(newPool);

    return newPool;
}

std::map<vk::DescriptorType, uint32_t> DescriptorAllocator::getDescriptorCounts(vk::DescriptorSetLayout layout) const
{
    std::map<vk::DescriptorType, uint32_t> counts;

    // 如果有布局缓存，从中查询绑定信息
    if (m_layoutCache)
    {
        auto bindings = m_layoutCache->getLayoutBindings(layout);

        // 统计每种描述符类型的数量
        for (const auto &binding : bindings)
        {
            counts[binding.descriptorType] += binding.descriptorCount;
        }
    }

    return counts;
}

// ========================================
// DescriptorLayoutBuilder 类的实现
// ========================================

DescriptorLayoutBuilder DescriptorLayoutBuilder::begin(DescriptorLayoutCache *cache)
{
    return DescriptorLayoutBuilder(cache);
}

DescriptorLayoutBuilder &DescriptorLayoutBuilder::addBinding(uint32_t binding, vk::DescriptorType type,
                                                             vk::ShaderStageFlags stageFlags, uint32_t count)
{
    vk::DescriptorSetLayoutBinding layoutBinding = {};
    layoutBinding.binding = binding;
    layoutBinding.descriptorType = type;
    layoutBinding.descriptorCount = count;
    layoutBinding.stageFlags = stageFlags;
    layoutBinding.pImmutableSamplers = nullptr; // 不使用不可变采样器

    m_bindings.push_back(layoutBinding);
    return *this; // 返回自身以支持链式调用
}

vk::DescriptorSetLayout DescriptorLayoutBuilder::build()
{
    if (!m_cache)
    {
        throw std::runtime_error("DescriptorLayoutCache is null in DescriptorLayoutBuilder");
    }
    return m_cache->createDescriptorLayout(m_bindings);
}

DescriptorLayoutBuilder::DescriptorLayoutBuilder(DescriptorLayoutCache *cache) : m_cache(cache)
{
}

// ========================================
// DescriptorUpdater 类的实现
// ========================================

DescriptorUpdater::DescriptorUpdater(Device &device, vk::DescriptorSet set) : m_device(device), m_set(set)
{
}

DescriptorUpdater DescriptorUpdater::begin(Device &device, vk::DescriptorSet set)
{
    return DescriptorUpdater(device, set);
}

DescriptorUpdater &DescriptorUpdater::writeBuffer(uint32_t binding, vk::DescriptorType type,
                                                  const vk::DescriptorBufferInfo &bufferInfo, uint32_t count)
{
    vk::WriteDescriptorSet write = {};
    write.dstSet = m_set;
    write.dstBinding = binding;
    write.dstArrayElement = 0; // 从数组的第一个元素开始写入
    write.descriptorCount = count;
    write.descriptorType = type;
    write.pBufferInfo = &bufferInfo;
    write.pImageInfo = nullptr;
    write.pTexelBufferView = nullptr;

    m_writes.push_back(write);
    return *this; // 返回自身以支持链式调用
}

DescriptorUpdater &DescriptorUpdater::writeImage(uint32_t binding, vk::DescriptorType type,
                                                 const vk::DescriptorImageInfo &imageInfo, uint32_t count)
{
    vk::WriteDescriptorSet write = {};
    write.dstSet = m_set;
    write.dstBinding = binding;
    write.dstArrayElement = 0; // 从数组的第一个元素开始写入
    write.descriptorCount = count;
    write.descriptorType = type;
    write.pImageInfo = &imageInfo;
    write.pBufferInfo = nullptr;
    write.pTexelBufferView = nullptr;

    m_writes.push_back(write);
    return *this; // 返回自身以支持链式调用
}

void DescriptorUpdater::update()
{
    // 批量提交所有写入操作到设备
    m_device.get().updateDescriptorSets(static_cast<uint32_t>(m_writes.size()), m_writes.data(), 0, nullptr);
}

} // namespace vkcore