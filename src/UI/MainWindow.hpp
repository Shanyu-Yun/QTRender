#pragma once

#include <QMainWindow>

class VulkanContainer;

/**
 * @brief 主窗口类，仅用于布局，Vulkan渲染在VulkanContainer中进行
 */
class MainWindow : public QMainWindow
{
    Q_OBJECT

  public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

    /**
     * @brief 获取Vulkan容器
     * @return VulkanContainer指针
     */
    VulkanContainer *getVulkanContainer() const
    {
        return m_vulkanContainer;
    }

  private:
    VulkanContainer *m_vulkanContainer;
};
