#include "VulkanWindow.hpp"
#include <iostream>

#ifdef _WIN32
#include <windows.h>
#endif

VulkanWindow::VulkanWindow(QWindow *parent) : QWindow(parent), m_windowReady(false)
{
    setSurfaceType(QWindow::VulkanSurface);
    resize(800, 600);
}

VulkanWindow::~VulkanWindow()
{
    // 析构函数为空，Qt会自动清理
}

void VulkanWindow::setVulkanConfig(const VulkanConfig &config)
{
    m_vulkanConfig = config;
}

QVulkanInstance *VulkanWindow::createVulkanInstance()
{
    auto *instance = new QVulkanInstance();

    // Set API version
    QVersionNumber apiVersion =
        QVersionNumber::fromString(QString::number(VK_VERSION_MAJOR(m_vulkanConfig.apiVersion)) + "." +
                                   QString::number(VK_VERSION_MINOR(m_vulkanConfig.apiVersion)) + "." +
                                   QString::number(VK_VERSION_PATCH(m_vulkanConfig.apiVersion)));
    instance->setApiVersion(apiVersion);

    // Set extensions
    QByteArrayList extensions;
    for (const auto &ext : m_vulkanConfig.instanceExtensions)
    {
        extensions << QByteArray::fromStdString(ext);
    }
    instance->setExtensions(extensions);

    // Set validation layers
    if (m_vulkanConfig.enableValidationLayers)
    {
        QByteArrayList layers;
        for (const auto &layer : m_vulkanConfig.validationLayers)
        {
            layers << QByteArray::fromStdString(layer);
        }
        instance->setLayers(layers);
    }

    // Create instance
    if (!instance->create())
    {
        std::cerr << "VulkanWindow: 创建QVulkanInstance失败" << std::endl;
        delete instance;
        return nullptr;
    }
    return instance;
}

VkSurfaceKHR VulkanWindow::createVulkanSurface(QVulkanInstance *instance)
{
    if (!instance)
    {
        std::cerr << "VulkanWindow: QVulkanInstance为空" << std::endl;
        return VK_NULL_HANDLE;
    }

    // 首先设置Vulkan Instance到QWindow
    this->setVulkanInstance(instance);

    VkSurfaceKHR surface = instance->surfaceForWindow(this);

    if (surface == VK_NULL_HANDLE)
    {
        std::cerr << "VulkanWindow: 创建Vulkan Surface失败" << std::endl;
        return VK_NULL_HANDLE;
    }

    return surface;
}

void VulkanWindow::showEvent(QShowEvent *event)
{
    QWindow::showEvent(event);

    if (!m_windowReady)
    {
        m_windowReady = true;
    }
}