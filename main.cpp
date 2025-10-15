#include "RenderCore/VulkanCore/public/Device.hpp"
#include "UI/MainWindow.hpp"
#include "UI/VulkanContainer.hpp"
#include "UI/VulkanWindow.hpp"
#include <QApplication>
#include <iostream>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    // 创建主窗口
    MainWindow mainWindow;

    // 在外部创建VulkanWindow
    VulkanWindow *vulkanWindow = new VulkanWindow();

    // 将VulkanWindow设置到容器中
    mainWindow.getVulkanContainer()->setVulkanWindow(vulkanWindow);

    // 显示主窗口
    mainWindow.show();

    // 创建Vulkan Instance
    QVulkanInstance *vulkanInstance = vulkanWindow->createVulkanInstance();
    if (!vulkanInstance)
    {
        std::cerr << "创建Vulkan Instance失败" << std::endl;
        return -1;
    }

    // 创建Vulkan Surface
    VkSurfaceKHR surface = vulkanWindow->createVulkanSurface(vulkanInstance);
    if (surface == VK_NULL_HANDLE)
    {
        std::cerr << "创建Vulkan Surface失败" << std::endl;
        delete vulkanInstance;
        return -1;
    }

    vk::Instance vkInstance = vulkanInstance->vkInstance();
    vkcore::Device::Config deviceConfig;
    deviceConfig.deviceExtensions = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};
    deviceConfig.vulkan1_3_features = {"dynamicRendering"};
    vkcore::Device device(vkInstance, deviceConfig);
    std::cout << device.GetPhysicalDevice().getProperties().deviceName << std::endl;

    // 运行应用程序
    int result = app.exec();

    // 清理资源
    delete vulkanInstance;

    return result;
}