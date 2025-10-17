/**
 * @file Descriptor.hpp
 * @author Summer
 * @brief 描述符布局缓存、描述符分配器及相关辅助类的头文件。
 *
 * 该文件定义了 Vulkan 描述符系统的核心组件，包括：
 * - DescriptorLayoutCache：描述符集布局的缓存管理，避免重复创建相同布局
 * - DescriptorAllocator：描述符集的池化分配器，支持自动扩展和重用
 * - DescriptorLayoutBuilder：描述符集布局的构建器模式辅助类
 * - DescriptorUpdater：描述符集更新的辅助类，简化缓冲区和图像描述符的写入
 *
 * 注意：DescriptorAllocator 可选地持有 DescriptorLayoutCache 指针以支持类型验证。
 * @version 1.0
 * @date 2025-10-18
 */

#pragma once

#include "Device.hpp"
#include <map>
#include <mutex>
#include <unordered_map>
#include <vector>
#include <vulkan/vulkan.hpp>

namespace vkcore
{
/**
 * @class DescriptorLayoutCache
 * @brief 描述符集布局缓存类，用于避免重复创建相同的描述符集布局。
 *
 * 该类使用哈希表缓存描述符集布局，当请求创建布局时，首先检查缓存中是否已存在
 * 相同绑定配置的布局。如果存在则直接返回缓存的布局，否则创建新布局并加入缓存。
 * 同时维护布局到绑定信息的反向映射，用于描述符分配器的类型验证。
 *
 * 线程安全：所有公共方法均使用互斥锁保护。
 */
class DescriptorLayoutCache
{
  public:
    /**
     * @brief 构造函数。
     * @param device 逻辑设备引用
     */
    DescriptorLayoutCache(Device &device);

    /**
     * @brief 析构函数，自动调用 cleanup() 清理资源。
     */
    ~DescriptorLayoutCache();

    /**
     * @brief 创建或获取描述符集布局。
     *
     * 根据绑定配置查找缓存，如果缓存命中则直接返回，否则创建新布局并加入缓存。
     * 同时维护布局到绑定信息的反向映射供后续查询。
     *
     * @param bindings 描述符集布局绑定列表
     * @return vk::DescriptorSetLayout 描述符集布局句柄
     */
    vk::DescriptorSetLayout createDescriptorLayout(const std::vector<vk::DescriptorSetLayoutBinding> &bindings);

    /**
     * @brief 获取布局的绑定信息（用于验证分配器容量）。
     *
     * 从反向映射中查询布局对应的绑定信息，用于描述符分配器验证池容量。
     *
     * @param layout 描述符集布局句柄
     * @return std::vector<vk::DescriptorSetLayoutBinding> 绑定信息列表，若未找到则返回空向量
     */
    std::vector<vk::DescriptorSetLayoutBinding> getLayoutBindings(vk::DescriptorSetLayout layout) const;

    /**
     * @brief 清理所有缓存的描述符集布局。
     *
     * 销毁所有已创建的描述符集布局并清空缓存。
     */
    void cleanup();

  private:
    /**
     * @struct LayoutInfoHash
     * @brief 描述符集布局绑定列表的哈希函数对象。
     *
     * 用于 std::unordered_map 的键哈希，基于绑定的关键字段（binding 索引、
     * 描述符类型、数量、着色器阶段标志）计算组合哈希值。
     */
    struct LayoutInfoHash
    {
        /**
         * @brief 计算绑定列表的哈希值。
         * @param bindings 描述符集布局绑定列表
         * @return std::size_t 哈希值
         */
        std::size_t operator()(const std::vector<vk::DescriptorSetLayoutBinding> &bindings) const;
    };

    /**
     * @struct LayoutInfoEqual
     * @brief 描述符集布局绑定列表的相等性比较函数对象。
     *
     * 用于 std::unordered_map 的键比较，逐个比较绑定的关键字段是否完全一致。
     */
    struct LayoutInfoEqual
    {
        /**
         * @brief 比较两个绑定列表是否相等。
         * @param a 第一个绑定列表
         * @param b 第二个绑定列表
         * @return bool 相等返回 true，否则返回 false
         */
        bool operator()(const std::vector<vk::DescriptorSetLayoutBinding> &a,
                        const std::vector<vk::DescriptorSetLayoutBinding> &b) const;
    };

  private:
    /// 逻辑设备引用
    Device &m_device;

    /// 布局缓存：绑定列表 -> 布局句柄
    std::unordered_map<std::vector<vk::DescriptorSetLayoutBinding>, vk::DescriptorSetLayout, LayoutInfoHash,
                       LayoutInfoEqual>
        m_layoutcache;

    /// 反向映射：布局句柄 -> 绑定列表（用于后续查询）
    std::unordered_map<vk::DescriptorSetLayout, std::vector<vk::DescriptorSetLayoutBinding>> m_layoutBindings;

    /// 互斥锁，保护线程安全
    mutable std::mutex m_mtx;
};

/**
 * @class DescriptorAllocator
 * @brief 描述符集分配器类，负责描述符池的管理和描述符集的分配。
 *
 * 该类使用池化策略管理描述符集的分配，支持自动扩展和重用。当前池不足时会自动
 * 创建新池或从空闲池列表中获取。支持批量分配和单个分配两种模式。
 *
 * 可选地持有 DescriptorLayoutCache 指针，用于在分配时查询布局的描述符类型信息，
 * 实现池容量的类型验证（当前框架已就绪，可扩展集成）。
 *
 * 线程安全：所有公共方法均使用互斥锁保护。
 */
class DescriptorAllocator
{
  public:
    /**
     * @brief 构造函数。
     * @param device 逻辑设备引用
     * @param layoutCache 可选的布局缓存指针，用于查询布局信息进行类型验证
     */
    DescriptorAllocator(Device &device, DescriptorLayoutCache *layoutCache = nullptr);

    /**
     * @brief 析构函数，自动调用 cleanup() 清理资源。
     */
    ~DescriptorAllocator();

    /**
     * @brief 重置所有描述符池。
     *
     * 将已使用的池重置并归还到空闲池列表，用于下次分配重用。
     * 不会销毁池本身，仅清空池中的描述符集。
     */
    void resetPools();

    /**
     * @brief 分配单个描述符集。
     *
     * 从当前池中分配一个描述符集，如果池不足会自动获取新池并重试。
     *
     * @param layout 描述符集布局
     * @return vk::DescriptorSet 描述符集句柄
     * @throws std::runtime_error 分配失败时抛出异常
     */
    vk::DescriptorSet allocate(vk::DescriptorSetLayout layout);

    /**
     * @brief 批量分配描述符集。
     *
     * 从当前池中分配多个相同布局的描述符集，如果池不足会自动获取新池并重试。
     * 支持 OutOfPoolMemoryError 和 FragmentedPoolError 的异常恢复。
     *
     * @param count 要分配的描述符集数量
     * @param layout 描述符集布局
     * @return std::vector<vk::DescriptorSet> 描述符集句柄列表
     * @throws std::runtime_error 分配失败时抛出异常
     */
    std::vector<vk::DescriptorSet> allocate(uint32_t count, vk::DescriptorSetLayout layout);

    /**
     * @brief 清理所有描述符池。
     *
     * 销毁所有已使用和空闲的描述符池，释放 Vulkan 资源。
     */
    void cleanup();

  private:
    /**
     * @brief 获取一个可用的描述符池。
     *
     * 优先从空闲池列表中获取，如果无空闲池则创建新池。新池包含多种常用描述符类型
     * （组合图像采样器、Uniform 缓冲区、Storage 缓冲区等），每种类型分配充足容量。
     *
     * @return vk::DescriptorPool 描述符池句柄
     */
    vk::DescriptorPool grabPool();

    /**
     * @brief 从布局中提取描述符类型信息（用于验证池容量）。
     *
     * 通过布局缓存查询布局的绑定信息，统计每种描述符类型的总数量。
     * 返回的映射可用于验证当前池是否有足够容量分配该布局。
     *
     * @param layout 描述符集布局
     * @return std::map<vk::DescriptorType, uint32_t> 描述符类型到数量的映射
     */
    std::map<vk::DescriptorType, uint32_t> getDescriptorCounts(vk::DescriptorSetLayout layout) const;

  private:
    /// 逻辑设备引用
    Device &m_device;

    /// 可选的布局缓存指针，用于查询布局信息
    DescriptorLayoutCache *m_layoutCache;

    /// 当前正在使用的描述符池
    vk::DescriptorPool m_currentPool = nullptr;

    /// 已使用的描述符池列表
    std::vector<vk::DescriptorPool> m_usedPools;

    /// 空闲的描述符池列表（已重置，可重用）
    std::vector<vk::DescriptorPool> m_freePools;

    /// 互斥锁，保护线程安全
    mutable std::mutex m_mtx;
};

/**
 * @class DescriptorLayoutBuilder
 * @brief 描述符集布局构建器类，使用构建器模式简化布局创建。
 *
 * 该类提供链式调用接口，方便地添加多个绑定并一次性创建描述符集布局。
 * 内部调用 DescriptorLayoutCache 实现布局的缓存和创建。
 *
 * 使用示例：
 * @code
 * auto layout = DescriptorLayoutBuilder::begin(&cache)
 *     .addBinding(0, vk::DescriptorType::eUniformBuffer, vk::ShaderStageFlagBits::eVertex)
 *     .addBinding(1, vk::DescriptorType::eCombinedImageSampler, vk::ShaderStageFlagBits::eFragment)
 *     .build();
 * @endcode
 */
class DescriptorLayoutBuilder
{
  public:
    /**
     * @brief 开始构建描述符集布局。
     * @param cache 布局缓存指针，用于缓存和创建布局
     * @return DescriptorLayoutBuilder 构建器对象
     */
    static DescriptorLayoutBuilder begin(DescriptorLayoutCache *cache);

    /**
     * @brief 添加一个描述符绑定。
     *
     * 支持链式调用，可连续添加多个绑定。
     *
     * @param binding 绑定索引
     * @param type 描述符类型
     * @param stageFlags 着色器阶段标志
     * @param count 描述符数量（默认为 1）
     * @return DescriptorLayoutBuilder& 自身引用，支持链式调用
     */
    DescriptorLayoutBuilder &addBinding(uint32_t binding, vk::DescriptorType type, vk::ShaderStageFlags stageFlags,
                                        uint32_t count = 1);

    /**
     * @brief 构建描述符集布局。
     *
     * 根据已添加的绑定创建或获取缓存的描述符集布局。
     *
     * @return vk::DescriptorSetLayout 描述符集布局句柄
     * @throws std::runtime_error 如果布局缓存为空
     */
    vk::DescriptorSetLayout build();

  private:
    /**
     * @brief 私有构造函数。
     * @param cache 布局缓存指针
     */
    DescriptorLayoutBuilder(DescriptorLayoutCache *cache);

  private:
    /// 布局缓存指针
    DescriptorLayoutCache *m_cache;

    /// 绑定列表
    std::vector<vk::DescriptorSetLayoutBinding> m_bindings;
};

/**
 * @class DescriptorUpdater
 * @brief 描述符集更新辅助类，简化描述符集的写入操作。
 *
 * 该类提供链式调用接口，方便地写入缓冲区和图像描述符，最后一次性提交更新。
 * 内部维护临时的写入信息列表，调用 update() 时批量提交到设备。
 *
 * 使用示例：
 * @code
 * DescriptorUpdater::begin(device, descriptorSet)
 *     .writeBuffer(0, vk::DescriptorType::eUniformBuffer, bufferInfo)
 *     .writeImage(1, vk::DescriptorType::eCombinedImageSampler, imageInfo)
 *     .update();
 * @endcode
 */
class DescriptorUpdater
{
  public:
    /**
     * @brief 开始更新描述符集。
     * @param device 逻辑设备引用
     * @param set 要更新的描述符集
     * @return DescriptorUpdater 更新器对象
     */
    static DescriptorUpdater begin(Device &device, vk::DescriptorSet set);

    /**
     * @brief 写入缓冲区描述符。
     *
     * 支持链式调用，可连续写入多个缓冲区描述符。
     *
     * @param binding 绑定索引
     * @param type 描述符类型（应为缓冲区相关类型）
     * @param bufferInfo 缓冲区描述符信息
     * @param count 描述符数量（默认为 1）
     * @return DescriptorUpdater& 自身引用，支持链式调用
     */
    DescriptorUpdater &writeBuffer(uint32_t binding, vk::DescriptorType type,
                                   const vk::DescriptorBufferInfo &bufferInfo, uint32_t count = 1);

    /**
     * @brief 写入图像描述符。
     *
     * 支持链式调用，可连续写入多个图像描述符。
     *
     * @param binding 绑定索引
     * @param type 描述符类型（应为图像相关类型）
     * @param imageInfo 图像描述符信息
     * @param count 描述符数量（默认为 1）
     * @return DescriptorUpdater& 自身引用，支持链式调用
     */
    DescriptorUpdater &writeImage(uint32_t binding, vk::DescriptorType type, const vk::DescriptorImageInfo &imageInfo,
                                  uint32_t count = 1);

    /**
     * @brief 提交所有描述符写入操作。
     *
     * 将之前通过 writeBuffer() 和 writeImage() 添加的写入操作批量提交到设备。
     */
    void update();

  private:
    /**
     * @brief 私有构造函数。
     * @param device 逻辑设备引用
     * @param set 要更新的描述符集
     */
    DescriptorUpdater(Device &device, vk::DescriptorSet set);

  private:
    /// 逻辑设备引用
    Device &m_device;

    /// 要更新的描述符集
    vk::DescriptorSet m_set;

    /// 写入操作列表
    std::vector<vk::WriteDescriptorSet> m_writes;
};
} // namespace vkcore