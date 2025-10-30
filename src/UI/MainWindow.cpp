#include "MainWindow.hpp"
#include "VulkanContainer.hpp"

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent), m_vulkanContainer(nullptr)
{
    setWindowTitle("Vulkan渲染器");
    setMinimumSize(800, 600);

    // 创建Vulkan容器
    m_vulkanContainer = new VulkanContainer(this);
    m_vulkanContainer->setMinimumSize(800, 600);

    setCentralWidget(m_vulkanContainer);
}

MainWindow::~MainWindow()
{
    // 析构函数为空，Qt会自动清理子对象
}
