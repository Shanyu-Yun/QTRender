#pragma once

#include <QVulkanDeviceFunctions>
#include <QVulkanInstance>
#include <QWindow>
#include <memory>
#include <string>
#include <vector>

// Vulkan头文件
#include <vulkan/vulkan.h>

/**
 * @brief Vulkan配置结构体
 */
struct VulkanConfig
{
    uint32_t apiVersion = VK_API_VERSION_1_4;
    std::vector<std::string> instanceExtensions = {
        VK_KHR_SURFACE_EXTENSION_NAME,
        "VK_KHR_win32_surface", // Windows平台
    };
    std::vector<std::string> deviceExtensions = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};
    std::vector<std::string> validationLayers = {"VK_LAYER_KHRONOS_validation"};
    bool enableValidationLayers = true;
    std::string applicationName = "Vulkan Renderer";
    uint32_t applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    std::string engineName = "Custom Engine";
    uint32_t engineVersion = VK_MAKE_VERSION(1, 0, 0);
};

/**
 * @brief Vulkan Window，仅负责Vulkan Instance创建和Surface获取
 */
class VulkanWindow : public QWindow
{
    Q_OBJECT

  public:
    explicit VulkanWindow(QWindow *parent = nullptr);
    ~VulkanWindow() override;

    /**
     * @brief 设置Vulkan配置
     * @param config Vulkan配置结构体
     */
    void setVulkanConfig(const VulkanConfig &config);

    /**
     * @brief 获取Vulkan配置
     * @return Vulkan配置结构体引用
     */
    const VulkanConfig &getVulkanConfig() const
    {
        return m_vulkanConfig;
    }

    /**
     * @brief 创建Vulkan Instance（调用者管理生命周期）
     * @return QVulkanInstance指针
     */
    QVulkanInstance *createVulkanInstance();

    /**
     * @brief 使用现有的QVulkanInstance创建Surface
     * @param instance QVulkanInstance指针
     * @return VkSurfaceKHR句柄
     */
    VkSurfaceKHR createVulkanSurface(QVulkanInstance *instance);

  protected:
    void showEvent(QShowEvent *event) override;

  private:
    VulkanConfig m_vulkanConfig;
    bool m_windowReady;
};