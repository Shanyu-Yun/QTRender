#pragma once
#include "Device.hpp"
#include "VKResource.hpp"
#include <memory>
#include <vector>
#include <vulkan/vulkan.hpp>

/**
 * @file SwapChain.hpp
 * @brief Vulkan 交换链的 RAII 封装，管理表面图像呈现
 * @details 该类封装了 Vulkan 交换链的创建、图像获取、呈现以及帧同步逻辑。
 *          使用双缓冲机制（MAX_FRAMES_IN_FLIGHT），自动处理图像可用信号量和飞行中栅栏。
 *          支持交换链过期时的自动重建。
 */

namespace vkcore
{

/**
 * @class SwapChain
 * @brief Vulkan 交换链管理类，提供表面图像获取和呈现功能
 * @warning 不支持拷贝和移动，使用引用或指针管理生命周期
 */
class SwapChain
{
  public:
    /// @brief 飞行中的最大帧数（双缓冲）
    static constexpr int MAX_FRAMES_IN_FLIGHT = 2;

    /**
     * @brief 构造函数，创建交换链及相关资源
     * @param surface Vulkan 表面句柄
     * @param device 逻辑设备引用
     * @param allocator VMA 分配器（用于未来可能的资源分配）
     * @throws std::runtime_error 如果交换链创建失败
     */
    SwapChain(vk::SurfaceKHR surface, Device &device, VmaAllocator allocator);

    /**
     * @brief 析构函数，自动清理交换链及同步对象
     */
    ~SwapChain();

    /** 禁用拷贝和移动 */
    SwapChain(const SwapChain &) = delete;
    SwapChain &operator=(const SwapChain &) = delete;

    /**
     * @brief 获取下一个可用的交换链图像索引
     * @param[out] imageIndex 输出参数，返回可用图像的索引
     * @return vk::Result 操作结果
     * @retval eSuccess 成功获取图像
     * @retval eErrorOutOfDateKHR 交换链过期，已自动重建
     * @retval eSuboptimalKHR 交换链次优但可用
     * @details 该函数会：
     *          1. 等待当前帧的栅栏
     *          2. 从交换链获取下一个图像索引
     *          3. 重置栅栏以供下次使用
     *          4. 如果交换链过期则自动重建
     * @throws std::runtime_error 如果获取图像失败
     */
    vk::Result acquireNextImage(uint32_t &imageIndex);

    /**
     * @brief 将渲染完成的图像呈现到表面
     * @param renderFinishedSemaphore 渲染完成信号量，确保渲染已完成
     * @param imageIndex 要呈现的图像索引
     * @return vk::Result 操作结果
     * @retval eSuccess 成功呈现图像
     * @retval eErrorOutOfDateKHR 交换链过期，已自动重建
     * @retval eSuboptimalKHR 交换链次优但已呈现
     * @details 该函数会等待渲染完成信号量，然后将图像提交到呈现队列
     * @throws std::runtime_error 如果呈现失败
     */
    vk::Result present(vk::Semaphore renderFinishedSemaphore, uint32_t imageIndex);

    /**
     * @brief 获取指定索引的交换链图像句柄
     * @param index 图像索引（0 到 getImageCount()-1）
     * @return vk::Image 图像句柄
     * @warning 不检查索引越界，调用者需确保索引有效
     */
    inline vk::Image getImage(uint32_t index) const
    {
        return m_images[index];
    }

    /**
     * @brief 获取指定索引的图像视图
     * @param index 图像索引
     * @return vk::ImageView 图像视图句柄
     */
    inline vk::ImageView getImageView(uint32_t index) const
    {
        return m_imageViews[index];
    }

    /**
     * @brief 获取交换链图像格式
     * @return vk::Format 图像格式（通常为 B8G8R8A8_SRGB）
     */
    inline vk::Format getSwapchainFormat() const
    {
        return m_swapchainFormat;
    }

    /**
     * @brief 获取交换链图像尺寸
     * @return vk::Extent2D 图像尺寸（宽度和高度）
     */
    inline vk::Extent2D getSwapchainExtent() const
    {
        return m_swapchainExtent;
    }

    /**
     * @brief 获取指定帧索引的图像可用信号量
     * @param index 帧索引（0 到 MAX_FRAMES_IN_FLIGHT-1）
     * @return vk::Semaphore 图像可用信号量，用于同步图像获取
     */
    inline vk::Semaphore getImageAvailableSemaphore(uint32_t index) const
    {
        return m_imageAvailableSemaphores[index];
    }

    /**
     * @brief 获取指定帧索引的飞行中栅栏
     * @param index 帧索引（0 到 MAX_FRAMES_IN_FLIGHT-1）
     * @return vk::Fence 飞行中栅栏，用于 CPU-GPU 同步
     */
    inline vk::Fence getFlightFence(uint32_t index) const
    {
        return m_FlightFences[index];
    }

    /**
     * @brief 获取飞行中栅栏（别名，与getFlightFence相同）
     * @param index 帧索引（0 到 MAX_FRAMES_IN_FLIGHT-1）
     * @return vk::Fence 飞行中栅栏
     */
    inline vk::Fence getInFlightFence(uint32_t index) const
    {
        return m_FlightFences[index];
    }

    /**
     * @brief 获取渲染完成信号量
     * @param index 帧索引（0 到 MAX_FRAMES_IN_FLIGHT-1）
     * @return vk::Semaphore 渲染完成信号量
     */
    inline vk::Semaphore getRenderFinishedSemaphore(uint32_t index) const
    {
        return m_renderFinishedSemaphores[index];
    }

    /**
     * @brief 获取当前帧索引
     * @return uint32_t 当前帧索引（0 或 1）
     */
    inline uint32_t getCurrentFrameIndex() const
    {
        return m_currentFrameIndex;
    }

    /**
     * @brief 推进到下一帧
     * @details 将帧索引递增并循环（0 -> 1 -> 0）
     */
    inline void advanceToNextFrame()
    {
        m_currentFrameIndex = (m_currentFrameIndex + 1) % MAX_FRAMES_IN_FLIGHT;
    }

    /**
     * @brief 清理交换链及相关资源
     * @details 销毁同步对象、图像视图和交换链本身
     */
    void cleanup();

  private:
    uint32_t m_currentFrameIndex = 0; ///< 当前渲染帧索引（0 或 1）

    std::vector<vk::Image> m_images;                       ///< 交换链图像句柄（由交换链拥有）
    std::vector<vk::ImageView> m_imageViews;               ///< 图像视图（由本类创建和销毁）
    std::vector<vk::Semaphore> m_imageAvailableSemaphores; ///< 图像可用信号量（每个交换链图像一个）
    std::vector<vk::Semaphore> m_renderFinishedSemaphores; ///< 渲染完成信号量（每个交换链图像一个）
    std::vector<vk::Fence> m_FlightFences;                 ///< 飞行中栅栏（每帧一个，MAX_FRAMES_IN_FLIGHT）
    std::vector<vk::Fence> m_imagesInFlight; ///< 跟踪每个图像是否正在使用（初始为 nullptr）

    Device &m_device;             ///< 逻辑设备引用
    vk::SwapchainKHR m_swapchain; ///< 交换链句柄
    vk::SurfaceKHR m_surface;     ///< 表面句柄
    VmaAllocator m_allocator;     ///< VMA 分配器

    vk::Format m_swapchainFormat = vk::Format::eUndefined; ///< 交换链图像格式
    vk::Extent2D m_swapchainExtent = {0, 0};               ///< 交换链图像尺寸

  private:
    /**
     * @brief 初始化交换链及相关资源
     * @details 依次调用 createswapchain、createimages、createsyncobjects
     */
    void init();

    /**
     * @brief 创建交换链
     * @details 查询表面能力、选择格式和呈现模式、创建交换链
     */
    void createswapchain();

    /**
     * @brief 创建图像视图
     * @details 获取交换链图像并为每个图像创建 ImageView
     */
    void createimages();

    /**
     * @brief 创建同步对象
     * @details 创建信号量和栅栏用于帧同步
     */
    void createsyncobjects();

    /**
     * @brief 重建交换链
     * @details 等待设备空闲，清理旧资源，重新初始化
     * @note 用于窗口大小改变或交换链过期时
     */
    void recreate();
};

} // namespace vkcore
