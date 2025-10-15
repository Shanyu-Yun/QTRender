#include "VulkanContainer.hpp"
#include "VulkanWindow.hpp"
#include <QVBoxLayout>

VulkanContainer::VulkanContainer(QWidget *parent) : QWidget(parent), m_vulkanWindow(nullptr)
{
    // 构造函数中不创建VulkanWindow，等待外部设置
}

VulkanContainer::~VulkanContainer()
{
    // 析构函数为空，Qt会自动清理子对象
}

void VulkanContainer::setVulkanWindow(VulkanWindow *vulkanWindow)
{
    if (m_vulkanWindow == vulkanWindow)
        return;

    // 清理旧的容器
    if (m_vulkanWindow)
    {
        // 删除旧的容器widget
        QWidget *oldContainer = findChild<QWidget *>();
        if (oldContainer)
        {
            oldContainer->deleteLater();
        }
    }

    m_vulkanWindow = vulkanWindow;

    if (m_vulkanWindow)
    {
        // 使用QWidget::createWindowContainer将QWindow嵌入到QWidget中
        QWidget *container = QWidget::createWindowContainer(m_vulkanWindow, this);
        container->setMinimumSize(800, 600);

        // 设置布局
        QVBoxLayout *layout = new QVBoxLayout(this);
        layout->addWidget(container);
        layout->setContentsMargins(0, 0, 0, 0);
    }
}
