#pragma once

#include <QWidget>

class VulkanWindow;

/**
 * @brief Vulkan容器类，用于将外部创建的VulkanWindow嵌入到QWidget体系中
 */
class VulkanContainer : public QWidget
{
    Q_OBJECT

  public:
    explicit VulkanContainer(QWidget *parent = nullptr);
    ~VulkanContainer() override;

    /**
     * @brief 设置Vulkan Window（由外部创建）
     * @param vulkanWindow 外部创建的VulkanWindow指针
     */
    void setVulkanWindow(VulkanWindow *vulkanWindow);

    /**
     * @brief 获取Vulkan Window
     * @return VulkanWindow指针
     */
    VulkanWindow *getVulkanWindow() const
    {
        return m_vulkanWindow;
    }

  private:
    VulkanWindow *m_vulkanWindow;
};
