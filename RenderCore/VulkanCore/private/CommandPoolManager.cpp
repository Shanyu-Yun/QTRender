#include "../public/CommandPoolManager.hpp"
#include <cassert>
#include <cstdio>
#include <stdexcept>
#include <string>

/**
 * @file CommandPoolManager.cpp
 * @brief CommandPoolManager 类的实现文件
 */

namespace vkcore
{

// thread_local 静态成员定义
thread_local std::shared_ptr<ThreadCommandPool> CommandPoolManager::t_threadPool = nullptr;

// ==================== CommandBufferDeleter 实现 ====================

void CommandBufferDeleter::operator()(vk::CommandBuffer *buffer) const
{
    if (pool && buffer && *buffer)
    {
        pool->recycle(*buffer, level);
        delete buffer;
    }
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

std::shared_ptr<ThreadCommandPool> CommandPoolManager::createthreadcommandpool()
{
    vk::CommandPoolCreateInfo poolInfo{};
    poolInfo.flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer; // 允许单独重置命令缓冲区
    poolInfo.queueFamilyIndex = m_queueFamilyIndex;

    vk::CommandPool commandPool = m_device.get().createCommandPool(poolInfo);
    if (!commandPool)
    {
        throw std::runtime_error("Failed to create command pool for thread " +
                                 std::to_string(std::hash<std::thread::id>{}(std::this_thread::get_id())));
    }

    return std::make_shared<ThreadCommandPool>(commandPool);
}

std::shared_ptr<ThreadCommandPool> CommandPoolManager::getorcreatethreadpool()
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
    auto newPool = createthreadcommandpool();

    {
        std::lock_guard<std::mutex> lock(m_mtx);
        m_threadPools[threadId] = newPool;
    }

    t_threadPool = newPool;
    return t_threadPool;
}

vk::CommandPool CommandPoolManager::getCommandPool()
{
    auto pool = getorcreatethreadpool();
    return pool->pool;
}

vk::CommandBuffer CommandPoolManager::allocateinternal(vk::CommandBufferLevel level)
{
    auto threadPool = getorcreatethreadpool();

    // 先尝试从对象池中获取
    auto &freeBuffers =
        (level == vk::CommandBufferLevel::ePrimary) ? threadPool->freePrimaryBuffers : threadPool->freeSecondaryBuffers;

    if (!freeBuffers.empty())
    {
        vk::CommandBuffer buffer = freeBuffers.front();
        freeBuffers.pop();

        // 重置命令缓冲区
        buffer.reset(vk::CommandBufferResetFlags{});
        return buffer;
    }

    // 对象池为空，分配新的
    vk::CommandBufferAllocateInfo allocInfo{};
    allocInfo.commandPool = threadPool->pool;
    allocInfo.level = level;
    allocInfo.commandBufferCount = 1;

    std::vector<vk::CommandBuffer> buffers = m_device.get().allocateCommandBuffers(allocInfo);
    threadPool->allocatedCount++;

    return buffers[0];
}

CommandBufferHandle CommandPoolManager::allocate(vk::CommandBufferLevel level)
{
    auto threadPool = getorcreatethreadpool();
    threadPool->inUseCount++; // 增加使用计数

    vk::CommandBuffer buffer = allocateinternal(level);
    vk::CommandBuffer *bufferPtr = new vk::CommandBuffer(buffer);
    return CommandBufferHandle(bufferPtr, CommandBufferDeleter{this, level});
}

std::vector<CommandBufferHandle> CommandPoolManager::allocateBatch(uint32_t count, vk::CommandBufferLevel level)
{
    auto threadPool = getorcreatethreadpool();
    threadPool->inUseCount += count; // 增加使用计数

    std::vector<CommandBufferHandle> handles;
    handles.reserve(count);

    auto &freeBuffers =
        (level == vk::CommandBufferLevel::ePrimary) ? threadPool->freePrimaryBuffers : threadPool->freeSecondaryBuffers;

    // 先从对象池中获取
    uint32_t reusedCount = 0;
    while (reusedCount < count && !freeBuffers.empty())
    {
        vk::CommandBuffer buffer = freeBuffers.front();
        freeBuffers.pop();
        buffer.reset(vk::CommandBufferResetFlags{});

        vk::CommandBuffer *bufferPtr = new vk::CommandBuffer(buffer);
        handles.emplace_back(bufferPtr, CommandBufferDeleter{this, level});
        reusedCount++;
    }

    // 如果对象池不够，批量分配新的
    uint32_t needAllocate = count - reusedCount;
    if (needAllocate > 0)
    {
        vk::CommandBufferAllocateInfo allocInfo{};
        allocInfo.commandPool = threadPool->pool;
        allocInfo.level = level;
        allocInfo.commandBufferCount = needAllocate;

        std::vector<vk::CommandBuffer> newBuffers = m_device.get().allocateCommandBuffers(allocInfo);

        for (auto buffer : newBuffers)
        {
            vk::CommandBuffer *bufferPtr = new vk::CommandBuffer(buffer);
            handles.emplace_back(bufferPtr, CommandBufferDeleter{this, level});
        }

        threadPool->allocatedCount += needAllocate;
    }

    return handles;
}

void CommandPoolManager::recycle(vk::CommandBuffer buffer, vk::CommandBufferLevel level)
{
    // 回收到当前线程的对象池
    if (!t_threadPool || !buffer)
    {
        return;
    }

    // 减少使用计数
    t_threadPool->inUseCount--;

    if (level == vk::CommandBufferLevel::ePrimary)
    {
        t_threadPool->freePrimaryBuffers.push(buffer);
    }
    else
    {
        t_threadPool->freeSecondaryBuffers.push(buffer);
    }
}

void CommandPoolManager::executeOnetime(vk::Queue queue, std::function<void(vk::CommandBuffer)> recordFunc)
{
    CommandBufferHandle cmd = allocate();

    // 开始记录
    vk::CommandBufferBeginInfo beginInfo{};
    beginInfo.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit;
    cmd->begin(beginInfo);

    // 执行用户提供的记录函数
    recordFunc(*cmd);

    // 结束记录
    cmd->end();

    // 提交
    vk::SubmitInfo submitInfo{};
    submitInfo.setCommandBuffers(*cmd);

    queue.submit(submitInfo, nullptr);
    queue.waitIdle();

    // cmd 析构时自动回收
}

void CommandPoolManager::submit(vk::Queue queue, const std::vector<CommandBufferHandle> &commandBuffers,
                                const std::vector<vk::Semaphore> &waitSemaphores,
                                const std::vector<vk::PipelineStageFlags> &waitStages,
                                const std::vector<vk::Semaphore> &signalSemaphores, vk::Fence fence)
{
    if (commandBuffers.empty())
    {
        return;
    }

    // 从 CommandBufferHandle 中提取 vk::CommandBuffer
    std::vector<vk::CommandBuffer> buffers;
    buffers.reserve(commandBuffers.size());
    for (const auto &handle : commandBuffers)
    {
        if (handle)
        {
            buffers.push_back(*handle);
        }
    }

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

    queue.submit(submitInfo, fence);
}

void CommandPoolManager::resetCommandPool(std::thread::id threadId)
{
    std::lock_guard<std::mutex> lock(m_mtx);
    auto it = m_threadPools.find(threadId);
    if (it != m_threadPools.end())
    {
        auto &threadPool = it->second;

        // ⚠️ 检查是否还有命令缓冲区在使用中
        if (threadPool->inUseCount > 0)
        {
            throw std::runtime_error("Cannot reset command pool: " + std::to_string(threadPool->inUseCount.load()) +
                                     " command buffers are still in use!");
        }

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
        m_device.get().resetCommandPool(threadPool->pool, vk::CommandPoolResetFlags{});
        threadPool->allocatedCount = 0;
    }
}

void CommandPoolManager::cleanup()
{
    std::lock_guard<std::mutex> lock(m_mtx);

    for (auto &[threadId, threadPool] : m_threadPools)
    {
        // ⚠️ 检查是否还有命令缓冲区在使用中
        if (threadPool->inUseCount > 0)
        {
            // 可以选择抛出异常或记录警告
            // 这里记录警告,因为程序退出时可能无法避免
            // 实际项目中应使用日志系统
            fprintf(stderr, "Warning: CommandPool cleanup with %zu command buffers still in use (thread %zu)\n",
                    threadPool->inUseCount.load(), std::hash<std::thread::id>{}(threadId));
        }

        if (threadPool && threadPool->pool)
        {
            m_device.get().destroyCommandPool(threadPool->pool);
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
