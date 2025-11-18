/**
 * @file RDGSyncInfo.cpp
 * @brief RDGFrameSyncManager 实现
 */

#include "../public/RDGSyncInfo.hpp"
#include <stdexcept>

namespace rendercore
{

// ==================== RDGFrameSyncManager 实现 ====================

RDGFrameSyncManager::RDGFrameSyncManager(vk::Device device, size_t maxFramesInFlight)
    : m_device(device), m_maxFramesInFlight(maxFramesInFlight)
{
    if (maxFramesInFlight == 0)
    {
        throw std::invalid_argument("RDGFrameSyncManager: maxFramesInFlight must be > 0");
    }

    // 预分配容器
    m_frameSyncInfos.resize(maxFramesInFlight);
    m_inFlightFences.reserve(maxFramesInFlight);
    m_imageAvailableSemaphores.reserve(maxFramesInFlight);
    m_renderFinishedSemaphores.reserve(maxFramesInFlight);

    // 创建同步原语
    vk::FenceCreateInfo fenceInfo{};
    fenceInfo.flags = vk::FenceCreateFlagBits::eSignaled; // 初始状态为已触发，避免第一帧等待

    vk::SemaphoreCreateInfo semaphoreInfo{};

    try
    {
        for (size_t i = 0; i < maxFramesInFlight; ++i)
        {
            // 创建 Fence
            vk::Fence fence = m_device.createFence(fenceInfo);
            m_inFlightFences.push_back(fence);

            // 创建信号量
            vk::Semaphore imageAvailable = m_device.createSemaphore(semaphoreInfo);
            vk::Semaphore renderFinished = m_device.createSemaphore(semaphoreInfo);
            m_imageAvailableSemaphores.push_back(imageAvailable);
            m_renderFinishedSemaphores.push_back(renderFinished);

            // 设置帧同步信息
            m_frameSyncInfos[i].executionFence = fence;
        }
    }
    catch (const vk::SystemError &e)
    {
        // 清理已创建的资源
        for (auto fence : m_inFlightFences)
        {
            m_device.destroyFence(fence);
        }
        for (auto sem : m_imageAvailableSemaphores)
        {
            m_device.destroySemaphore(sem);
        }
        for (auto sem : m_renderFinishedSemaphores)
        {
            m_device.destroySemaphore(sem);
        }
        throw std::runtime_error("RDGFrameSyncManager: Failed to create sync primitives: " + std::string(e.what()));
    }
}

RDGFrameSyncManager::~RDGFrameSyncManager()
{
    // 等待所有操作完成
    try
    {
        waitAll();
    }
    catch (...)
    {
        // 析构函数中不抛出异常
    }

    // 销毁同步原语
    for (auto fence : m_inFlightFences)
    {
        m_device.destroyFence(fence);
    }
    for (auto sem : m_imageAvailableSemaphores)
    {
        m_device.destroySemaphore(sem);
    }
    for (auto sem : m_renderFinishedSemaphores)
    {
        m_device.destroySemaphore(sem);
    }
}

RDGSyncInfo &RDGFrameSyncManager::getCurrentFrameSync()
{
    return m_frameSyncInfos[m_currentFrame];
}

void RDGFrameSyncManager::advanceFrame()
{
    // 移动到下一帧
    m_currentFrame = (m_currentFrame + 1) % m_maxFramesInFlight;

    // 等待下一帧的 Fence（确保资源可以安全复用）
    vk::Fence nextFence = m_inFlightFences[m_currentFrame];
    vk::Result result = m_device.waitForFences(nextFence, VK_TRUE, UINT64_MAX);

    if (result != vk::Result::eSuccess)
    {
        throw std::runtime_error("RDGFrameSyncManager::advanceFrame: Failed to wait for fence");
    }

    // 重置 Fence 以便下次使用
    m_device.resetFences(nextFence);

    // 清除旧的等待/信号信息（Fence 保留）
    auto &syncInfo = m_frameSyncInfos[m_currentFrame];
    vk::Fence fence = syncInfo.executionFence.value();
    syncInfo.clear();
    syncInfo.executionFence = fence;
}

void RDGFrameSyncManager::waitAll()
{
    if (m_inFlightFences.empty())
    {
        return;
    }

    vk::Result result = m_device.waitForFences(m_inFlightFences, VK_TRUE, UINT64_MAX);

    if (result != vk::Result::eSuccess)
    {
        throw std::runtime_error("RDGFrameSyncManager::waitAll: Failed to wait for fences");
    }
}

vk::Fence RDGFrameSyncManager::getCurrentFence() const
{
    return m_inFlightFences[m_currentFrame];
}

std::pair<vk::Semaphore, vk::Semaphore> RDGFrameSyncManager::getSwapChainSemaphores(size_t frameIndex)
{
    if (frameIndex >= m_maxFramesInFlight)
    {
        throw std::out_of_range("RDGFrameSyncManager::getSwapChainSemaphores: Invalid frame index");
    }

    return {m_imageAvailableSemaphores[frameIndex], m_renderFinishedSemaphores[frameIndex]};
}

} // namespace rendercore
