#pragma once

#include "Device.hpp"
#include <atomic>
#include <functional>
#include <memory>
#include <mutex>
#include <queue>
#include <thread>
#include <unordered_map>
#include <vector>

namespace vkcore
{

// 前置声明
class CommandPoolManager;

/**
 * @struct CommandBufferDeleter
 * @brief 自定义删除器，用于 unique_ptr，自动回收命令缓冲区到对象池
 */
struct CommandBufferDeleter
{
    CommandPoolManager *pool = nullptr;
    vk::CommandBufferLevel level = vk::CommandBufferLevel::ePrimary;

    void operator()(vk::CommandBuffer *buffer) const;
};

/**
 * @typedef CommandBufferHandle
 * @brief 使用 unique_ptr 实现的 RAII 命令缓冲区句柄
 */
using CommandBufferHandle = std::unique_ptr<vk::CommandBuffer, CommandBufferDeleter>;

/**
 * @struct ThreadCommandPool
 * @brief 每个线程的命令池及其缓冲区池
 */
struct ThreadCommandPool
{
    vk::CommandPool pool;                               ///< Vulkan 命令池
    std::queue<vk::CommandBuffer> freePrimaryBuffers;   ///< 空闲的主命令缓冲区
    std::queue<vk::CommandBuffer> freeSecondaryBuffers; ///< 空闲的次级命令缓冲区
    std::atomic<size_t> allocatedCount{0};              ///< 已分配的总数（统计用）
    std::atomic<size_t> inUseCount{0}; ///< 正在使用中的命令缓冲区数量（防止悬空引用）

    ThreadCommandPool(vk::CommandPool p) : pool(p)
    {
    }
};

/**
 * @class CommandPoolManager
 * @brief 管理每个线程的命令池，提供线程安全的命令缓冲区分配与复用
 */
class CommandPoolManager
{
  public:
    /**
     * @brief 构造函数
     * @param device Device 引用（不拥有，需确保生命周期）
     * @param queueFamilyIndex 命令池关联的队列族索引
     */
    CommandPoolManager(Device &device, uint32_t queueFamilyIndex);
    ~CommandPoolManager();

    /** 禁用拷贝与移动 */
    CommandPoolManager(const CommandPoolManager &) = delete;
    CommandPoolManager &operator=(const CommandPoolManager &) = delete;
    CommandPoolManager(CommandPoolManager &&) = delete;
    CommandPoolManager &operator=(CommandPoolManager &&) = delete;

    // ==================== 核心分配接口 ====================

    /**
     * @brief 分配单个命令缓冲区（返回 RAII Handle，自动回收）
     * @param level 命令缓冲区级别
     * @return CommandBufferHandle RAII 封装，析构时自动回收
     */
    CommandBufferHandle allocate(vk::CommandBufferLevel level = vk::CommandBufferLevel::ePrimary);

    /**
     * @brief 批量分配命令缓冲区（RAII Handle 版本）
     * @param count 分配数量
     * @param level 命令缓冲区级别
     * @return std::vector<CommandBufferHandle> RAII 封装数组
     */
    std::vector<CommandBufferHandle> allocateBatch(uint32_t count,
                                                   vk::CommandBufferLevel level = vk::CommandBufferLevel::ePrimary);

    // ==================== 便捷接口 ====================

    /**
     * @brief 执行一次性命令（自动管理生命周期）
     * @param queue 提交队列
     * @param recordFunc 记录命令的回调函数，接收 vk::CommandBuffer 参数
     */
    void executeOnetime(vk::Queue queue, std::function<void(vk::CommandBuffer)> recordFunc);

    /**
     * @brief 提交命令缓冲区到队列
     * @param queue 目标队列
     * @param commandBuffers 命令缓冲区句柄数组
     * @param waitSemaphores 等待的信号量
     * @param waitStages 等待阶段
     * @param signalSemaphores 完成后触发的信号量
     * @param fence 完成后触发的 Fence
     */
    void submit(vk::Queue queue, const std::vector<CommandBufferHandle> &commandBuffers,
                const std::vector<vk::Semaphore> &waitSemaphores = {},
                const std::vector<vk::PipelineStageFlags> &waitStages = {},
                const std::vector<vk::Semaphore> &signalSemaphores = {}, vk::Fence fence = nullptr);

    // ==================== 命令池管理 ====================

    /**
     * @brief 获取当前线程的命令池，若不存在则创建
     * @return vk::CommandPool 当前线程的命令池句柄
     */
    vk::CommandPool getCommandPool();

    /**
     * @brief 重置指定线程的命令池（会清空所有缓冲区池）
     * @param threadId 线程ID，默认为当前线程
     */
    void resetCommandPool(std::thread::id threadId = std::this_thread::get_id());

    /**
     * @brief 清理所有命令池资源
     */
    void cleanup();

    // ==================== 统计与调试 ====================

    /**
     * @struct PoolStats
     * @brief 命令池统计信息
     */
    struct PoolStats
    {
        size_t totalThreadPools;      ///< 线程命令池总数
        size_t totalAllocatedBuffers; ///< 已分配的命令缓冲区总数
        size_t totalFreeBuffers;      ///< 空闲命令缓冲区总数
    };

    /**
     * @brief 获取统计信息（调试用）
     */
    PoolStats getStats() const;

  private:
    /**
     * @brief 为当前线程创建命令池
     * @return std::shared_ptr<ThreadCommandPool> 新创建的线程命令池
     */
    std::shared_ptr<ThreadCommandPool> createThreadCommandPool();

    /**
     * @brief 获取或创建当前线程的命令池
     */
    std::shared_ptr<ThreadCommandPool> getOrCreateThreadPool();

    /**
     * @brief 内部实现：从对象池或新分配获取命令缓冲区
     */
    vk::CommandBuffer allocateInternal(vk::CommandBufferLevel level);

    /**
     * @brief 回收命令缓冲区到对象池（由 CommandBufferDeleter 调用）
     * @param buffer 要回收的命令缓冲区
     * @param level 命令缓冲区级别
     */
    void recycle(vk::CommandBuffer buffer, vk::CommandBufferLevel level);

  private:
    Device &m_device;            ///< Device 引用
    uint32_t m_queueFamilyIndex; ///< 队列族索引
    mutable std::mutex m_mtx;    ///< 保护共享数据的互斥锁
    std::unordered_map<std::thread::id, std::shared_ptr<ThreadCommandPool>> m_threadPools; ///< 线程ID到命令池的映射

    /**
     * @brief 线程局部缓存，避免每次查找 map
     */
    static thread_local std::shared_ptr<ThreadCommandPool> t_threadPool;

    friend struct CommandBufferDeleter;
};

} // namespace vkcore
