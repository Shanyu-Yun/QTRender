/**
 * @file SwapChain.cpp
 * @author Summer
 * @brief SwapChain 类的实现文件
 * @details 该文件包含 SwapChain 类的成员函数实现，负责 Vulkan 交换链的创建、
 *          图像获取、呈现以及帧同步管理。实现了交换链过期时的自动重建机制。
 * @version 1.0
 * @date 2025-01-17
 */

#include "SwapChain.hpp"

namespace vkcore
{
SwapChain::SwapChain(vk::SurfaceKHR surface, Device &device, VmaAllocator allocator)
    : m_surface(surface), m_device(device), m_allocator(allocator)
{
    init();
}

SwapChain::~SwapChain()
{
    cleanup();
}

vk::Result SwapChain::acquireNextImage(uint32_t &imageIndex)
{
    // 等待当前帧的栅栏
    [[maybe_unused]] vk::Result waitResult =
        m_device.get().waitForFences(1, &m_FlightFences[m_currentFrameIndex], VK_TRUE, UINT64_MAX);

    // 获取下一个图像索引
    vk::Result result = m_device.get().acquireNextImageKHR(
        m_swapchain, UINT64_MAX, m_imageAvailableSemaphores[m_currentFrameIndex], nullptr, &imageIndex);

    // 如果交换链过期，需要重建
    if (result == vk::Result::eErrorOutOfDateKHR)
    {
        recreate();
        return result;
    }
    else if (result != vk::Result::eSuccess && result != vk::Result::eSuboptimalKHR)
    {
        throw std::runtime_error("Failed to acquire swap chain image!");
    }

    // 重置栅栏
    [[maybe_unused]] vk::Result resetResult = m_device.get().resetFences(1, &m_FlightFences[m_currentFrameIndex]);

    return result;
}

vk::Result SwapChain::present(vk::Semaphore renderFinishedSemaphore, uint32_t imageIndex)
{
    vk::PresentInfoKHR presentInfo = {};
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = &renderFinishedSemaphore;
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = &m_swapchain;
    presentInfo.pImageIndices = &imageIndex;

    vk::Result result = m_device.getPresentQueue().presentKHR(presentInfo);

    if (result == vk::Result::eErrorOutOfDateKHR || result == vk::Result::eSuboptimalKHR)
    {
        recreate();
    }
    else if (result != vk::Result::eSuccess)
    {
        throw std::runtime_error("Failed to present swap chain image!");
    }

    return result;
}

void SwapChain::cleanup()
{
    // 销毁同步对象
    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
    {
        if (m_imageAvailableSemaphores[i])
        {
            m_device.get().destroySemaphore(m_imageAvailableSemaphores[i]);
        }
        if (m_FlightFences[i])
        {
            m_device.get().destroyFence(m_FlightFences[i]);
        }
    }
    m_imageAvailableSemaphores.clear();
    m_FlightFences.clear();

    // 销毁 ImageView（Image 由交换链管理，不需要销毁）
    for (auto imageView : m_imageViews)
    {
        if (imageView)
        {
            m_device.get().destroyImageView(imageView);
        }
    }
    m_imageViews.clear();
    m_images.clear();

    // 销毁交换链
    if (m_swapchain)
    {
        m_device.get().destroySwapchainKHR(m_swapchain);
        m_swapchain = nullptr;
    }
}

void SwapChain::init()
{
    createswapchain();
    createimages();
    createsyncobjects();
}

void SwapChain::createswapchain()
{
    // 查询表面能力
    vk::SurfaceCapabilitiesKHR capabilities = m_device.getPhysicalDevice().getSurfaceCapabilitiesKHR(m_surface);

    // 选择表面格式（优先 B8G8R8A8_SRGB + sRGB 色彩空间）
    std::vector<vk::SurfaceFormatKHR> formats = m_device.getPhysicalDevice().getSurfaceFormatsKHR(m_surface);
    vk::SurfaceFormatKHR selectedFormat = formats[0];
    for (const auto &format : formats)
    {
        if (format.format == vk::Format::eB8G8R8A8Srgb && format.colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear)
        {
            selectedFormat = format;
            break;
        }
    }

    // 选择呈现模式（优先 Mailbox > Immediate > FIFO）
    // Mailbox: 三缓冲，低延迟；Immediate: 立即呈现，可能撕裂；FIFO: 垂直同步，保证支持
    std::vector<vk::PresentModeKHR> presentModes = m_device.getPhysicalDevice().getSurfacePresentModesKHR(m_surface);
    vk::PresentModeKHR selectedPresentMode = vk::PresentModeKHR::eFifo;
    for (const auto &mode : presentModes)
    {
        if (mode == vk::PresentModeKHR::eMailbox)
        {
            selectedPresentMode = mode;
            break;
        }
        else if (mode == vk::PresentModeKHR::eImmediate)
        {
            selectedPresentMode = mode;
        }
    }

    // 确定交换链范围（图像尺寸）
    vk::Extent2D extent;
    if (capabilities.currentExtent.width != UINT32_MAX)
    {
        extent = capabilities.currentExtent;
    }
    else
    {
        extent.width = std::max(capabilities.minImageExtent.width, std::min(capabilities.maxImageExtent.width, 800u));
        extent.height =
            std::max(capabilities.minImageExtent.height, std::min(capabilities.maxImageExtent.height, 600u));
    }

    // 确定图像数量（建议 minImageCount + 1，实现三缓冲）
    uint32_t imageCount = capabilities.minImageCount + 1;
    if ((capabilities.maxImageCount > 0) && (imageCount > capabilities.maxImageCount))
    {
        imageCount = capabilities.maxImageCount;
    }

    // 创建交换链
    vk::SwapchainCreateInfoKHR createInfo;
    createInfo.setSurface(m_surface)
        .setMinImageCount(imageCount)
        .setImageFormat(selectedFormat.format)
        .setImageColorSpace(selectedFormat.colorSpace)
        .setImageExtent(extent)
        .setImageArrayLayers(1)
        .setImageUsage(vk::ImageUsageFlagBits::eColorAttachment);

    // 队列族索引处理
    // 如果图形队列和呈现队列不同，使用并发模式；否则使用独占模式（性能更好）
    uint32_t queueFamilyIndices[] = {m_device.getGraphicsQueueFamilyIndices(), m_device.getPresentQueueFamilyIndices()};

    if (queueFamilyIndices[0] != queueFamilyIndices[1])
    {
        createInfo.setImageSharingMode(vk::SharingMode::eConcurrent);
        createInfo.setQueueFamilyIndexCount(2);
        createInfo.setPQueueFamilyIndices(queueFamilyIndices);
    }
    else
    {
        createInfo.setImageSharingMode(vk::SharingMode::eExclusive);
    }

    createInfo.setPreTransform(capabilities.currentTransform)
        .setCompositeAlpha(vk::CompositeAlphaFlagBitsKHR::eOpaque)
        .setPresentMode(selectedPresentMode)
        .setClipped(VK_TRUE)
        .setOldSwapchain(nullptr);

    m_swapchain = m_device.get().createSwapchainKHR(createInfo);

    // 存储格式和尺寸供其他函数使用
    m_swapchainFormat = selectedFormat.format;
    m_swapchainExtent = extent;
}
void SwapChain::createimages()
{
    // 获取交换链图像（由交换链拥有，不需要手动销毁）
    m_images = m_device.get().getSwapchainImagesKHR(m_swapchain);

    // 为每个图像创建 ImageView（需要手动销毁）
    m_imageViews.clear();
    m_imageViews.reserve(m_images.size());

    for (const auto &image : m_images)
    {
        vk::ImageViewCreateInfo viewInfo = {};
        viewInfo.image = image;
        viewInfo.viewType = vk::ImageViewType::e2D;
        viewInfo.format = m_swapchainFormat;
        viewInfo.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
        viewInfo.subresourceRange.baseMipLevel = 0;
        viewInfo.subresourceRange.levelCount = 1;
        viewInfo.subresourceRange.baseArrayLayer = 0;
        viewInfo.subresourceRange.layerCount = 1;

        m_imageViews.push_back(m_device.get().createImageView(viewInfo));
    }
}

void SwapChain::createsyncobjects()
{
    m_imageAvailableSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
    m_FlightFences.resize(MAX_FRAMES_IN_FLIGHT);

    vk::SemaphoreCreateInfo semaphoreInfo;

    // 栅栏初始状态为已信号（signaled），避免第一帧等待死锁
    vk::FenceCreateInfo fenceInfo;
    fenceInfo.setFlags(vk::FenceCreateFlagBits::eSignaled);

    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
    {
        m_imageAvailableSemaphores[i] = m_device.get().createSemaphore(semaphoreInfo);
        m_FlightFences[i] = m_device.get().createFence(fenceInfo);
    }
}

void SwapChain::recreate()
{
    // 等待所有 GPU 操作完成
    m_device.get().waitIdle();

    // 销毁旧交换链及相关资源
    cleanup();

    // 重新初始化（会重新查询表面能力，自动获取新尺寸）
    init();
}
} // namespace vkcore
