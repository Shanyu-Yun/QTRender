#include "../public/CommandPoolManager.hpp"
#include <cassert>
#include <stdexcept>

/**
 * @file CommandPoolManager.cpp
 * @brief CommandPoolManager 类的实现文件
 *
 * 新设计核心特性：
 * 1. 对象池模式：复用命令缓冲区，减少分配开销
 * 2. 智能指针管理：自动回收，防止泄漏
 * 3. 状态验证：Debug 模式下检查操作合法性
 * 4. 批量操作：简化提交流程
 */

namespace vkcore
{

// thread_local 静态成员定义
thread_local std::shared_ptr<ThreadCommandPool> CommandPoolManager::t_threadPool = nullptr;

// ==================== CommandBuffer 基类实现 ====================

CommandBuffer::CommandBuffer(vk::CommandBuffer buffer, CommandPoolManager *pool, vk::CommandBufferLevel level)
    : m_buffer(buffer), m_poolManager(pool), m_level(level), m_state(CommandBufferState::Initial)
{
}

CommandBuffer::~CommandBuffer()
{
    // 析构时自动回收到对象池
    if (m_poolManager && m_buffer)
    {
        m_poolManager->recycleCommandBuffer(m_buffer, m_level);
    }
}

CommandBuffer::CommandBuffer(CommandBuffer &&other) noexcept
    : m_buffer(other.m_buffer), m_poolManager(other.m_poolManager), m_level(other.m_level), m_state(other.m_state)
{
    other.m_buffer = nullptr;
    other.m_poolManager = nullptr;
    other.m_state = CommandBufferState::Invalid;
}

CommandBuffer &CommandBuffer::operator=(CommandBuffer &&other) noexcept
{
    if (this != &other)
    {
        // 回收当前持有的缓冲区
        if (m_poolManager && m_buffer)
        {
            m_poolManager->recycleCommandBuffer(m_buffer, m_level);
        }

        m_buffer = other.m_buffer;
        m_poolManager = other.m_poolManager;
        m_level = other.m_level;
        m_state = other.m_state;

        other.m_buffer = nullptr;
        other.m_poolManager = nullptr;
        other.m_state = CommandBufferState::Invalid;
    }
    return *this;
}

void CommandBuffer::begin(vk::CommandBufferUsageFlags flags, const vk::CommandBufferInheritanceInfo *inheritanceInfo)
{
#ifdef _DEBUG
    // Debug 模式下验证状态
    if (m_state == CommandBufferState::Recording)
    {
        throw std::runtime_error("CommandBuffer::begin() called while already recording");
    }
    if (m_state == CommandBufferState::Pending)
    {
        throw std::runtime_error("CommandBuffer::begin() called while pending execution");
    }
#endif

    vk::CommandBufferBeginInfo beginInfo{};
    beginInfo.flags = flags;
    beginInfo.pInheritanceInfo = inheritanceInfo;

    m_buffer.begin(beginInfo);
    m_state = CommandBufferState::Recording;
}

void CommandBuffer::end()
{
#ifdef _DEBUG
    if (m_state != CommandBufferState::Recording)
    {
        throw std::runtime_error("CommandBuffer::end() called but not in recording state");
    }
#endif

    m_buffer.end();
    m_state = CommandBufferState::Executable;
}

void CommandBuffer::reset(vk::CommandBufferResetFlags flags)
{
    m_buffer.reset(flags);
    m_state = CommandBufferState::Initial;
}

// ==================== CommandPoolManager 实现 ====================

CommandPoolManager::CommandPoolManager(Device &device, uint32_t queueFamilyIndex)
    : m_device(device), m_queueFamilyIndex(queueFamilyIndex)
{
}

CommandPoolManager::~CommandPoolManager()
{
    cleanup();
}

std::shared_ptr<ThreadCommandPool> CommandPoolManager::createThreadCommandPool()
{
    vk::CommandPoolCreateInfo poolInfo{};
    poolInfo.flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer; // 允许单独重置命令缓冲区
    poolInfo.queueFamilyIndex = m_queueFamilyIndex;

    vk::CommandPool commandPool = m_device.Get().createCommandPool(poolInfo);
    if (!commandPool)
    {
        throw std::runtime_error("Failed to create command pool for thread " +
                                 std::to_string(std::hash<std::thread::id>{}(std::this_thread::get_id())));
    }

    return std::make_shared<ThreadCommandPool>(commandPool);
}

std::shared_ptr<ThreadCommandPool> CommandPoolManager::getOrCreateThreadPool()
{
    // 先检查 thread_local 缓存
    if (t_threadPool)
    {
        return t_threadPool;
    }

    // 如果缓存为空，从 map 中查找或创建
    std::thread::id threadId = std::this_thread::get_id();

    {
        std::lock_guard<std::mutex> lock(m_mtx);
        auto it = m_threadPools.find(threadId);
        if (it != m_threadPools.end())
        {
            t_threadPool = it->second;
            return t_threadPool;
        }
    }

    // 创建新的命令池
    auto newPool = createThreadCommandPool();

    {
        std::lock_guard<std::mutex> lock(m_mtx);
        m_threadPools[threadId] = newPool;
    }

    t_threadPool = newPool;
    return t_threadPool;
}

vk::CommandPool CommandPoolManager::getCommandPool()
{
    auto pool = getOrCreateThreadPool();
    return pool->pool;
}

std::vector<std::shared_ptr<PrimaryCommandBuffer>> CommandPoolManager::allocatePrimaryCommandBuffers(uint32_t count)
{
    auto threadPool = getOrCreateThreadPool();
    std::vector<std::shared_ptr<PrimaryCommandBuffer>> result;
    result.reserve(count);

    // 先从对象池中获取空闲的命令缓冲区
    uint32_t reusedCount = 0;
    while (reusedCount < count && !threadPool->freePrimaryBuffers.empty())
    {
        vk::CommandBuffer buffer = threadPool->freePrimaryBuffers.front();
        threadPool->freePrimaryBuffers.pop();

        // 重置命令缓冲区
        buffer.reset(vk::CommandBufferResetFlags{});

        result.push_back(std::make_shared<PrimaryCommandBuffer>(buffer, this, vk::CommandBufferLevel::ePrimary));
        reusedCount++;
    }

    // 如果对象池不够，分配新的
    uint32_t needAllocate = count - reusedCount;
    if (needAllocate > 0)
    {
        vk::CommandBufferAllocateInfo allocInfo{};
        allocInfo.commandPool = threadPool->pool;
        allocInfo.level = vk::CommandBufferLevel::ePrimary;
        allocInfo.commandBufferCount = needAllocate;

        std::vector<vk::CommandBuffer> buffers = m_device.Get().allocateCommandBuffers(allocInfo);

        for (auto &buffer : buffers)
        {
            result.push_back(std::make_shared<PrimaryCommandBuffer>(buffer, this, vk::CommandBufferLevel::ePrimary));
        }

        threadPool->allocatedCount += needAllocate;
    }

    return result;
}

std::vector<std::shared_ptr<SecondaryCommandBuffer>> CommandPoolManager::allocateSecondaryCommandBuffers(uint32_t count)
{
    auto threadPool = getOrCreateThreadPool();
    std::vector<std::shared_ptr<SecondaryCommandBuffer>> result;
    result.reserve(count);

    // 先从对象池中获取空闲的命令缓冲区
    uint32_t reusedCount = 0;
    while (reusedCount < count && !threadPool->freeSecondaryBuffers.empty())
    {
        vk::CommandBuffer buffer = threadPool->freeSecondaryBuffers.front();
        threadPool->freeSecondaryBuffers.pop();

        // 重置命令缓冲区
        buffer.reset(vk::CommandBufferResetFlags{});

        result.push_back(std::make_shared<SecondaryCommandBuffer>(buffer, this, vk::CommandBufferLevel::eSecondary));
        reusedCount++;
    }

    // 如果对象池不够，分配新的
    uint32_t needAllocate = count - reusedCount;
    if (needAllocate > 0)
    {
        vk::CommandBufferAllocateInfo allocInfo{};
        allocInfo.commandPool = threadPool->pool;
        allocInfo.level = vk::CommandBufferLevel::eSecondary;
        allocInfo.commandBufferCount = needAllocate;

        std::vector<vk::CommandBuffer> buffers = m_device.Get().allocateCommandBuffers(allocInfo);

        for (auto &buffer : buffers)
        {
            result.push_back(
                std::make_shared<SecondaryCommandBuffer>(buffer, this, vk::CommandBufferLevel::eSecondary));
        }

        threadPool->allocatedCount += needAllocate;
    }

    return result;
}

void CommandPoolManager::recycleCommandBuffer(vk::CommandBuffer buffer, vk::CommandBufferLevel level)
{
    // 回收到当前线程的对象池
    if (!t_threadPool)
    {
        return; // 线程已退出或命令池已销毁
    }

    if (level == vk::CommandBufferLevel::ePrimary)
    {
        t_threadPool->freePrimaryBuffers.push(buffer);
    }
    else
    {
        t_threadPool->freeSecondaryBuffers.push(buffer);
    }
}

void CommandPoolManager::submitCommands(vk::Queue queue,
                                        const std::vector<std::shared_ptr<PrimaryCommandBuffer>> &commands,
                                        const std::vector<vk::Semaphore> &waitSemaphores,
                                        const std::vector<vk::PipelineStageFlags> &waitStages,
                                        const std::vector<vk::Semaphore> &signalSemaphores, vk::Fence fence)
{
    // 收集命令缓冲区句柄
    std::vector<vk::CommandBuffer> buffers = collectBuffers(commands);

    if (buffers.empty())
    {
        return;
    }

    // 构建提交信息
    vk::SubmitInfo submitInfo{};
    submitInfo.setCommandBuffers(buffers);

    if (!waitSemaphores.empty())
    {
        submitInfo.setWaitSemaphores(waitSemaphores);
        submitInfo.setWaitDstStageMask(waitStages);
    }

    if (!signalSemaphores.empty())
    {
        submitInfo.setSignalSemaphores(signalSemaphores);
    }

    // 提交
    queue.submit(submitInfo, fence);

    // 标记命令缓冲区为 Pending 状态
#ifdef _DEBUG
    for (auto &cmd : commands)
    {
        if (cmd)
        {
            cmd->m_state = CommandBufferState::Pending;
        }
    }
#endif
}

std::shared_ptr<PrimaryCommandBuffer> CommandPoolManager::beginSingleTimeCommands()
{
    auto cmds = allocatePrimaryCommandBuffers(1);
    cmds[0]->begin(vk::CommandBufferUsageFlagBits::eOneTimeSubmit);
    return cmds[0];
}

void CommandPoolManager::endSingleTimeCommands(std::shared_ptr<PrimaryCommandBuffer> command, vk::Queue queue)
{
    command->end();

    // 提交并等待完成
    vk::SubmitInfo submitInfo{};
    vk::CommandBuffer buffer = command->getBuffer();
    submitInfo.setCommandBuffers(buffer);

    queue.submit(submitInfo, nullptr);
    queue.waitIdle(); // 等待执行完成

    // 命令缓冲区会在 shared_ptr 析构时自动回收
}

void CommandPoolManager::resetCommandPool(std::thread::id threadId)
{
    std::lock_guard<std::mutex> lock(m_mtx);
    auto it = m_threadPools.find(threadId);
    if (it != m_threadPools.end())
    {
        auto &threadPool = it->second;

        // 清空对象池
        while (!threadPool->freePrimaryBuffers.empty())
        {
            threadPool->freePrimaryBuffers.pop();
        }
        while (!threadPool->freeSecondaryBuffers.empty())
        {
            threadPool->freeSecondaryBuffers.pop();
        }

        // 重置命令池（会释放所有命令缓冲区）
        m_device.Get().resetCommandPool(threadPool->pool, vk::CommandPoolResetFlags{});
        threadPool->allocatedCount = 0;
    }
}

void CommandPoolManager::cleanup()
{
    std::lock_guard<std::mutex> lock(m_mtx);

    for (auto &[threadId, threadPool] : m_threadPools)
    {
        if (threadPool && threadPool->pool)
        {
            m_device.Get().destroyCommandPool(threadPool->pool);
        }
    }

    m_threadPools.clear();
    t_threadPool = nullptr;
}

CommandPoolManager::PoolStats CommandPoolManager::getStats() const
{
    std::lock_guard<std::mutex> lock(m_mtx);

    PoolStats stats{};
    stats.totalThreadPools = m_threadPools.size();

    for (const auto &[threadId, threadPool] : m_threadPools)
    {
        stats.totalAllocatedBuffers += threadPool->allocatedCount;
        stats.totalFreeBuffers += threadPool->freePrimaryBuffers.size();
        stats.totalFreeBuffers += threadPool->freeSecondaryBuffers.size();
    }

    return stats;
}

} // namespace vkcore
