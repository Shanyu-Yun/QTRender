#pragma once

#include "Device.hpp"
#include <atomic>
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
 * @enum CommandBufferState
 * @brief 命令缓冲区状态（用于 Debug 验证）
 */
enum class CommandBufferState
{
    Initial,    ///< 初始状态
    Recording,  ///< 正在记录
    Executable, ///< 可执行状态
    Pending,    ///< 已提交待执行
    Invalid     ///< 已失效
};

/**
 * @class CommandBuffer
 * @brief 命令缓冲区基类（主/次级共享）
 *
 * 核心改进：
 * - 生命周期管理：通过 shared_ptr 管理，安全回收
 * - 状态验证：Debug 模式下检查调用顺序
 * - 自动回收：析构时自动归还到对象池
 */
class CommandBuffer
{
  public:
    /**
     * @brief 构造函数
     * @param buffer Vulkan 命令缓冲区句柄
     * @param pool 所属的命令池管理器（用于回收）
     * @param level 命令缓冲区级别
     */
    CommandBuffer(vk::CommandBuffer buffer, CommandPoolManager *pool, vk::CommandBufferLevel level);
    virtual ~CommandBuffer();

    /** 禁用拷贝，允许移动 */
    CommandBuffer(const CommandBuffer &) = delete;
    CommandBuffer &operator=(const CommandBuffer &) = delete;
    CommandBuffer(CommandBuffer &&) noexcept;
    CommandBuffer &operator=(CommandBuffer &&) noexcept;

    /**
     * @brief 获取底层 Vulkan 命令缓冲区句柄
     * @return vk::CommandBuffer 句柄
     */
    vk::CommandBuffer getBuffer() const
    {
        return m_buffer;
    }

    /**
     * @brief 获取命令缓冲区级别
     */
    vk::CommandBufferLevel getLevel() const
    {
        return m_level;
    }

    /**
     * @brief 检查命令缓冲区是否有效
     */
    bool isValid() const
    {
        return m_buffer && m_state != CommandBufferState::Invalid;
    }

    /**
     * @brief 开始记录命令
     * @param flags 使用标志
     * @param inheritanceInfo 继承信息（仅次级命令缓冲区需要）
     */
    void begin(vk::CommandBufferUsageFlags flags = {},
               const vk::CommandBufferInheritanceInfo *inheritanceInfo = nullptr);

    /**
     * @brief 结束记录
     */
    void end();

    /**
     * @brief 重置命令缓冲区
     * @param flags 重置标志
     */
    void reset(vk::CommandBufferResetFlags flags = {});

    /**
     * @brief 获取当前状态（Debug 用）
     */
    CommandBufferState getState() const
    {
        return m_state;
    }

  protected:
    vk::CommandBuffer m_buffer;        ///< Vulkan 命令缓冲区句柄
    CommandPoolManager *m_poolManager; ///< 所属的命令池管理器
    vk::CommandBufferLevel m_level;    ///< 命令缓冲区级别
    CommandBufferState m_state;        ///< 当前状态（Debug 用）

    friend class CommandPoolManager;
};

/**
 * @class PrimaryCommandBuffer
 * @brief 主命令缓冲区（可直接提交到队列）
 */
class PrimaryCommandBuffer : public CommandBuffer
{
  public:
    using CommandBuffer::CommandBuffer;

    /**
     * @brief 开始记录（主命令缓冲区版本）
     */
    void begin(vk::CommandBufferUsageFlags flags = {})
    {
        CommandBuffer::begin(flags, nullptr);
    }
};

/**
 * @class SecondaryCommandBuffer
 * @brief 次级命令缓冲区（可在主命令缓冲区中执行）
 */
class SecondaryCommandBuffer : public CommandBuffer
{
  public:
    using CommandBuffer::CommandBuffer;

    /**
     * @brief 开始记录（次级命令缓冲区版本，需要继承信息）
     */
    void begin(vk::CommandBufferUsageFlags flags, const vk::CommandBufferInheritanceInfo &inheritanceInfo)
    {
        CommandBuffer::begin(flags | vk::CommandBufferUsageFlagBits::eRenderPassContinue, &inheritanceInfo);
    }
};

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

    ThreadCommandPool(vk::CommandPool p) : pool(p)
    {
    }
};

/**
 * @class CommandPoolManager
 * @brief 管理每个线程的命令池，提供线程安全的命令缓冲区分配与复用
 *
 * 核心改进：
 * 1. 对象池模式：复用命令缓冲区，避免频繁分配
 * 2. 智能指针管理：使用 shared_ptr 追踪生命周期
 * 3. 批量操作：提供 collectBuffers 等便捷方法
 * 4. 一次性命令：简化临时命令提交流程
 * 5. 状态验证：Debug 模式下检查操作合法性
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

    // ==================== 基础分配接口 ====================

    /**
     * @brief 分配主命令缓冲区（支持对象池复用）
     * @param count 分配数量
     * @return std::vector<std::shared_ptr<PrimaryCommandBuffer>> 主命令缓冲区数组
     */
    std::vector<std::shared_ptr<PrimaryCommandBuffer>> allocatePrimaryCommandBuffers(uint32_t count = 1);

    /**
     * @brief 分配次级命令缓冲区（支持对象池复用）
     * @param count 分配数量
     * @return std::vector<std::shared_ptr<SecondaryCommandBuffer>> 次级命令缓冲区数组
     */
    std::vector<std::shared_ptr<SecondaryCommandBuffer>> allocateSecondaryCommandBuffers(uint32_t count = 1);

    // ==================== 批量操作辅助 ====================

    /**
     * @brief 从命令缓冲区数组中收集 vk::CommandBuffer 句柄（用于提交）
     * @tparam T PrimaryCommandBuffer 或 SecondaryCommandBuffer
     * @param commands 命令缓冲区 shared_ptr 数组
     * @return std::vector<vk::CommandBuffer> Vulkan 句柄数组
     */
    template <typename T>
    static std::vector<vk::CommandBuffer> collectBuffers(const std::vector<std::shared_ptr<T>> &commands)
    {
        std::vector<vk::CommandBuffer> buffers;
        buffers.reserve(commands.size());
        for (const auto &cmd : commands)
        {
            if (cmd && cmd->isValid())
            {
                buffers.push_back(cmd->getBuffer());
            }
        }
        return buffers;
    }

    /**
     * @brief 提交命令缓冲区到队列
     * @param queue 目标队列
     * @param commands 命令缓冲区数组
     * @param waitSemaphores 等待的信号量
     * @param waitStages 等待阶段
     * @param signalSemaphores 完成后触发的信号量
     * @param fence 完成后触发的 Fence
     */
    void submitCommands(vk::Queue queue, const std::vector<std::shared_ptr<PrimaryCommandBuffer>> &commands,
                        const std::vector<vk::Semaphore> &waitSemaphores = {},
                        const std::vector<vk::PipelineStageFlags> &waitStages = {},
                        const std::vector<vk::Semaphore> &signalSemaphores = {}, vk::Fence fence = nullptr);

    // ==================== 一次性命令辅助 ====================

    /**
     * @brief 开始一次性命令（常用于数据传输）
     * @return std::shared_ptr<PrimaryCommandBuffer> 已开始记录的命令缓冲区
     */
    std::shared_ptr<PrimaryCommandBuffer> beginSingleTimeCommands();

    /**
     * @brief 结束并提交一次性命令（会立即等待执行完成）
     * @param command 命令缓冲区
     * @param queue 提交队列
     */
    void endSingleTimeCommands(std::shared_ptr<PrimaryCommandBuffer> command, vk::Queue queue);

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
     * @brief 回收命令缓冲区到对象池（由 CommandBuffer 析构时调用）
     * @param buffer 要回收的命令缓冲区
     * @param level 命令缓冲区级别
     */
    void recycleCommandBuffer(vk::CommandBuffer buffer, vk::CommandBufferLevel level);

  private:
    Device &m_device;            ///< Device 引用
    uint32_t m_queueFamilyIndex; ///< 队列族索引
    mutable std::mutex m_mtx;    ///< 保护共享数据的互斥锁（mutable 允许在 const 函数中锁定）
    std::unordered_map<std::thread::id, std::shared_ptr<ThreadCommandPool>> m_threadPools; ///< 线程ID到命令池的映射

    /**
     * @brief 线程局部缓存，避免每次查找 map
     */
    static thread_local std::shared_ptr<ThreadCommandPool> t_threadPool;

    friend class CommandBuffer;
};

} // namespace vkcore
