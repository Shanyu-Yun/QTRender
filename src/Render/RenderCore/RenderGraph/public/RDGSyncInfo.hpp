/**
 * @file RDGSyncInfo.hpp
 * @brief 渲染图同步信息 - 封装 Fence 和 Semaphore
 * @details 用于正确的 CPU-GPU 和 GPU-GPU 同步
 */

#pragma once

#include <optional>
#include <vector>
#include <vulkan/vulkan.hpp>

namespace rendercore
{

/**
 * @struct RDGWaitInfo
 * @brief 等待信息，用于在执行前等待某个信号量
 */
struct RDGWaitInfo
{
    vk::Semaphore semaphore;                                                  ///< 要等待的信号量
    vk::PipelineStageFlags waitStage = vk::PipelineStageFlagBits::eTopOfPipe; ///< 等待的管线阶段

    RDGWaitInfo() = default;
    RDGWaitInfo(vk::Semaphore sem, vk::PipelineStageFlags stage) : semaphore(sem), waitStage(stage)
    {
    }
};

/**
 * @struct RDGSyncInfo
 * @brief 渲染图同步信息
 * @details 封装执行渲染图所需的所有同步原语
 */
struct RDGSyncInfo
{
    // ==================== 输入同步（等待） ====================

    /**
     * @brief 需要等待的信号量列表（例如 SwapChain 的 imageAvailable）
     * @details 渲染图执行前会等待这些信号量
     */
    std::vector<RDGWaitInfo> waitSemaphores;

    // ==================== 输出同步（信号） ====================

    /**
     * @brief 执行完成后要触发的信号量列表（例如 SwapChain 的 renderFinished）
     * @details 渲染图执行完成后会触发这些信号量
     */
    std::vector<vk::Semaphore> signalSemaphores;

    /**
     * @brief 执行完成后要触发的 Fence（可选）
     * @details 用于 CPU 端等待 GPU 执行完成
     */
    std::optional<vk::Fence> executionFence;

    // ==================== 便利函数 ====================

    /**
     * @brief 添加等待信号量
     * @param semaphore 信号量
     * @param stage 等待阶段（默认为 ColorAttachmentOutput，适用于 SwapChain）
     */
    void addWaitSemaphore(vk::Semaphore semaphore,
                          vk::PipelineStageFlags stage = vk::PipelineStageFlagBits::eColorAttachmentOutput)
    {
        waitSemaphores.emplace_back(semaphore, stage);
    }

    /**
     * @brief 添加信号信号量
     * @param semaphore 信号量
     */
    void addSignalSemaphore(vk::Semaphore semaphore)
    {
        signalSemaphores.push_back(semaphore);
    }

    /**
     * @brief 设置执行 Fence
     * @param fence Fence 对象
     */
    void setExecutionFence(vk::Fence fence)
    {
        executionFence = fence;
    }

    /**
     * @brief 清除所有同步信息
     */
    void clear()
    {
        waitSemaphores.clear();
        signalSemaphores.clear();
        executionFence.reset();
    }

    /**
     * @brief 检查是否有任何同步原语
     */
    bool hasSyncPrimitives() const
    {
        return !waitSemaphores.empty() || !signalSemaphores.empty() || executionFence.has_value();
    }
};

/**
 * @class RDGFrameSyncManager
 * @brief 帧同步管理器 - 管理多帧并行（Frames in Flight）
 * @details 创建和管理用于多帧并行渲染的同步原语
 */
class RDGFrameSyncManager
{
  public:
    /**
     * @brief 构造函数
     * @param device Vulkan 设备
     * @param maxFramesInFlight 最大并行帧数（通常为 2 或 3）
     */
    RDGFrameSyncManager(vk::Device device, size_t maxFramesInFlight = 2);

    /**
     * @brief 析构函数
     */
    ~RDGFrameSyncManager();

    // 禁用拷贝
    RDGFrameSyncManager(const RDGFrameSyncManager &) = delete;
    RDGFrameSyncManager &operator=(const RDGFrameSyncManager &) = delete;

    /**
     * @brief 获取当前帧的同步信息
     * @return 当前帧的 RDGSyncInfo 引用
     */
    RDGSyncInfo &getCurrentFrameSync();

    /**
     * @brief 前进到下一帧
     * @details 会等待下一帧的 Fence，确保资源可以安全复用
     */
    void advanceFrame();

    /**
     * @brief 等待所有帧完成
     * @details 用于关闭前的清理
     */
    void waitAll();

    /**
     * @brief 获取当前帧索引
     */
    size_t getCurrentFrameIndex() const
    {
        return m_currentFrame;
    }

    /**
     * @brief 获取最大并行帧数
     */
    size_t getMaxFramesInFlight() const
    {
        return m_maxFramesInFlight;
    }

    /**
     * @brief 获取当前帧的 Fence
     */
    vk::Fence getCurrentFence() const;

    /**
     * @brief 为 SwapChain 集成创建信号量对
     * @param frameIndex 帧索引
     * @return pair<imageAvailable, renderFinished>
     */
    std::pair<vk::Semaphore, vk::Semaphore> getSwapChainSemaphores(size_t frameIndex);

  private:
    vk::Device m_device;
    size_t m_maxFramesInFlight;
    size_t m_currentFrame = 0;

    // 每帧的同步原语
    std::vector<RDGSyncInfo> m_frameSyncInfos;
    std::vector<vk::Fence> m_inFlightFences;
    std::vector<vk::Semaphore> m_imageAvailableSemaphores;
    std::vector<vk::Semaphore> m_renderFinishedSemaphores;
};

} // namespace rendercore
