#include "RenderCore/VulkanCore/public/CommandPoolManager.hpp"
#include "RenderCore/VulkanCore/public/Device.hpp"
#include "UI/MainWindow.hpp"
#include "UI/VulkanContainer.hpp"
#include "UI/VulkanWindow.hpp"
#include <QApplication>
#include <iostream>

void testRecord(vkcore::Device &device, vkcore::CommandPoolManager &commandPoolManager, vk::Queue graphicsQueue);

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
    vkcore::Device device(vkInstance, surface, deviceConfig);
    std::cout << device.GetPhysicalDevice().getProperties().deviceName << std::endl;

    try
    {
        vk::Queue graphicsQueue = device.GetGraphicsQueue();
        uint32_t graphicsQueueFamilyIndex = device.GetGraphicsQueueFamilyIndices();

        vkcore::CommandPoolManager commandPoolManager(device, graphicsQueueFamilyIndex);

        // 测试命令
        testRecord(device, commandPoolManager, graphicsQueue);

        std::cout << "Success" << std::endl;
    }
    catch (const std::exception &e)
    {
        std::cerr << "Vulkan操作失败: " << e.what() << std::endl;
    }

    // 运行应用程序
    int result = app.exec();

    // 清理资源
    device.Cleanup();
    delete vulkanInstance;

    return result;
}

void testRecord(vkcore::Device &device, vkcore::CommandPoolManager &commandPoolManager, vk::Queue graphicsQueue)
{
    vk::Device vkDevice = device.Get();

    // 0. 创建查询池用于时间戳
    vk::QueryPoolCreateInfo queryPoolInfo{};
    queryPoolInfo.queryType = vk::QueryType::eTimestamp;
    queryPoolInfo.queryCount = 2; // 开始和结束两个时间戳

    vk::QueryPool queryPool = vkDevice.createQueryPool(queryPoolInfo);

    commandPoolManager.executeOnetime(graphicsQueue, [&](vk::CommandBuffer cmd) {
        // 重置查询池
        cmd.resetQueryPool(queryPool, 0, 2);

        // 记录开始时间戳
        cmd.writeTimestamp(vk::PipelineStageFlagBits::eTopOfPipe, queryPool, 0);

        // 记录结束时间戳
        cmd.writeTimestamp(vk::PipelineStageFlagBits::eBottomOfPipe, queryPool, 1);
    });

    // 读取时间戳结果
    std::array<uint64_t, 2> timestamps;
    vk::Result result =
        vkDevice.getQueryPoolResults(queryPool,
                                     0,                  // 起始查询索引
                                     2,                  // 查询数量
                                     sizeof(timestamps), // 数据大小
                                     timestamps.data(),  // 输出数据
                                     sizeof(uint64_t),   // 每个查询的步长
                                     vk::QueryResultFlagBits::e64 | vk::QueryResultFlagBits::eWait // 64位结果并等待
        );

    if (result == vk::Result::eSuccess)
    {
        // 获取时间戳周期（纳秒）
        vk::PhysicalDeviceProperties props = device.GetPhysicalDevice().getProperties();
        float timestampPeriod = props.limits.timestampPeriod;

        uint64_t startTime = timestamps[0];
        uint64_t endTime = timestamps[1];

        // 计算执行时间（纳秒）
        double executionTimeNs = (endTime - startTime) * timestampPeriod;
        double executionTimeUs = executionTimeNs / 1000.0; // 微秒
        double executionTimeMs = executionTimeUs / 1000.0; // 毫秒

        std::cout << "开始时间戳: " << startTime << std::endl;
        std::cout << "结束时间戳: " << endTime << std::endl;
        std::cout << "GPU 执行时间: " << executionTimeUs << " 微秒 (" << executionTimeMs << " 毫秒)" << std::endl;
        std::cout << "时间戳周期: " << timestampPeriod << " 纳秒" << std::endl;
    }
    else
    {
        std::cerr << "警告: 无法读取时间戳结果" << std::endl;
    }

    // 4. 清理资源
    vkDevice.destroyQueryPool(queryPool);
}