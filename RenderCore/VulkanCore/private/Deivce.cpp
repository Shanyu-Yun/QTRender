#include "../public/Device.hpp"
#include <set>

/**
 * @file Device.cpp
 * @author Summer
 * @brief Device 类的实现文件。
 *
 * 该文件包含 Device 类的成员函数实现，负责 Vulkan 物理设备与逻辑设备的创建与管理。
 * 主要功能包括设备选择、队列族查询、逻辑设备创建以及资源清理等。
 * @version 1.0
 * @date 2025-10-12
 */

namespace vkcore
{

Device::Device(vk::Instance &instance, VkSurfaceKHR &surface, const Config &config)
    : m_instance(instance), m_surface(surface), m_config(config)
{
    selectphyscialdevice();
    findqueuefamilies();
    createlogicaldevice();
}

void Device::selectphyscialdevice()
{
    std::vector<vk::PhysicalDevice> devices = m_instance.enumeratePhysicalDevices();
    std::vector<int> deviceScores(devices.size(), 0);
    bool exsitPhysicalDevice = false;
    if (devices.empty())
    {
        throw std::runtime_error("Failed to find GPUs with Vulkan support!");
    }
    for (int i = 0; i < devices.size(); ++i)
    {
        if (checkdeviceextensionsupport(devices[i]) && checkvulkanfeaturessupport(devices[i]))
        {
            deviceScores[i] = ratedevicescore(devices[i]);
            exsitPhysicalDevice = true;
        }
    }

    if (!exsitPhysicalDevice)
    {
        throw std::runtime_error("Failed to find a suitable GPU!");
    }

    auto maxIt = std::max_element(deviceScores.begin(), deviceScores.end());
    size_t maxIndex = std::distance(deviceScores.begin(), maxIt);
    m_physicalDevice = devices[maxIndex];
}

void Device::findqueuefamilies()
{
    // 获取所有队列族属性
    std::vector<vk::QueueFamilyProperties> queueFamilies = m_physicalDevice.getQueueFamilyProperties();

    // 遍历队列族，查找支持图形和呈现的队列族
    uint32_t i = 0;
    for (const auto &queueFamily : queueFamilies)
    {
        // 检查是否支持图形操作
        if (queueFamily.queueFlags & vk::QueueFlagBits::eGraphics)
        {
            m_queueFamilyIndices.graphicsFamily = i;
        }

        // 检查是否支持呈现操作（使用 surface）
        VkBool32 presentSupport = false;
        vkGetPhysicalDeviceSurfaceSupportKHR(m_physicalDevice, i, m_surface, &presentSupport);
        if (presentSupport)
        {
            m_queueFamilyIndices.presentFamily = i;
        }

        // 如果两个队列族都找到了，提前退出
        if (m_queueFamilyIndices.isComplete())
        {
            break;
        }

        i++;
    }

    // 验证是否找到了所需的队列族
    if (!m_queueFamilyIndices.isComplete())
    {
        throw std::runtime_error("Failed to find required queue families!");
    }
}

void Device::createlogicaldevice()
{
    // 准备队列创建信息
    std::vector<vk::DeviceQueueCreateInfo> queueCreateInfos;
    // 使用 set 去重：如果图形和呈现是同一个队列族，只创建一次
    std::set<uint32_t> uniqueQueueFamilies = {m_queueFamilyIndices.graphicsFamily.value(),
                                              m_queueFamilyIndices.presentFamily.value()};

    float queuePriority = 1.0f;
    for (uint32_t queueFamily : uniqueQueueFamilies)
    {
        vk::DeviceQueueCreateInfo queueCreateInfo{};
        queueCreateInfo.queueFamilyIndex = queueFamily;
        queueCreateInfo.queueCount = 1;
        queueCreateInfo.pQueuePriorities = &queuePriority;
        queueCreateInfos.push_back(queueCreateInfo);
    }

    // 准备设备特性
    vk::PhysicalDeviceFeatures deviceFeatures{};

    // 准备设备创建信息
    vk::DeviceCreateInfo createInfo{};
    createInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
    createInfo.pQueueCreateInfos = queueCreateInfos.data();
    createInfo.pEnabledFeatures = &deviceFeatures;

    // 添加设备扩展
    std::vector<const char *> extensionNames;
    for (const auto &ext : m_config.deviceExtensions)
    {
        extensionNames.push_back(ext.c_str());
    }
    createInfo.enabledExtensionCount = static_cast<uint32_t>(extensionNames.size());
    createInfo.ppEnabledExtensionNames = extensionNames.data();

    // 创建逻辑设备
    m_device = m_physicalDevice.createDevice(createInfo);

    // 获取队列句柄
    // 注意：如果图形和呈现队列族相同，两次获取的是同一个队列（索引0）
    // 如果队列族不同，则分别从各自的队列族获取
    m_graphicsQueue = m_device.getQueue(m_queueFamilyIndices.graphicsFamily.value(), 0);
    m_presentQueue = m_device.getQueue(m_queueFamilyIndices.presentFamily.value(), 0);
}

bool Device::checkdeviceextensionsupport(vk::PhysicalDevice &device)
{
    std::vector<vk::ExtensionProperties> availableExtensions = device.enumerateDeviceExtensionProperties();

    bool isSupported = false;
    for (const std::string &requiredExt : m_config.deviceExtensions)
    {
        isSupported = false;
        for (const vk::ExtensionProperties &ext : availableExtensions)
        {
            if (std::strcmp(ext.extensionName, requiredExt.c_str()) == 0)
            {
                isSupported = true;
                break;
            }
        }
        if (!isSupported)
            return false;
    }
    return true;
}

bool Device::checkvulkanfeaturessupport(vk::PhysicalDevice &device)
{
    auto property = device.getProperties();

    if (!m_config.vulkan1_1_features.empty())
    {
        if (property.apiVersion < VK_API_VERSION_1_1)
        {
            return false;
        }
        for (const auto &feature : m_config.vulkan1_1_features)
        {
            if (!checkspeficfeaturesupport(device, feature))
            {
                return false;
            }
        }
    }

    if (!m_config.vulkan1_2_features.empty())
    {
        if (property.apiVersion < VK_API_VERSION_1_2)
        {
            return false;
        }
        for (const auto &feature : m_config.vulkan1_2_features)
        {
            if (!checkspeficfeaturesupport(device, feature))
            {
                return false;
            }
        }
    }

    if (!m_config.vulkan1_3_features.empty())
    {
        if (property.apiVersion < VK_API_VERSION_1_3)
        {
            return false;
        }
        for (const auto &feature : m_config.vulkan1_3_features)
        {
            if (!checkspeficfeaturesupport(device, feature))
            {
                return false;
            }
        }
    }

    return true;
}

bool Device::checkspeficfeaturesupport(vk::PhysicalDevice &device, std::string feature)
{

    // Vulkan 1.3 特性检查
    if (feature == "dynamicRendering")
    {
        auto features13 = device.getFeatures2<vk::PhysicalDeviceFeatures2, vk::PhysicalDeviceVulkan13Features>();
        return features13.get<vk::PhysicalDeviceVulkan13Features>().dynamicRendering == VK_TRUE;
    }

    if (feature == "synchronization2")
    {
        auto features13 = device.getFeatures2<vk::PhysicalDeviceFeatures2, vk::PhysicalDeviceVulkan13Features>();
        return features13.get<vk::PhysicalDeviceVulkan13Features>().synchronization2 == VK_TRUE;
    }

    if (feature == "maintenance4")
    {

        auto features13 = device.getFeatures2<vk::PhysicalDeviceFeatures2, vk::PhysicalDeviceVulkan13Features>();
        return features13.get<vk::PhysicalDeviceVulkan13Features>().maintenance4 == VK_TRUE;
    }

    // Vulkan 1.2 特性检查
    if (feature == "bufferDeviceAddress")
    {

        auto features12 = device.getFeatures2<vk::PhysicalDeviceFeatures2, vk::PhysicalDeviceVulkan12Features>();
        return features12.get<vk::PhysicalDeviceVulkan12Features>().bufferDeviceAddress == VK_TRUE;
    }

    if (feature == "descriptorIndexing")
    {
        auto features12 = device.getFeatures2<vk::PhysicalDeviceFeatures2, vk::PhysicalDeviceVulkan12Features>();
        return features12.get<vk::PhysicalDeviceVulkan12Features>().descriptorIndexing == VK_TRUE;
    }

    if (feature == "timelineSemaphore")
    {
        auto features12 = device.getFeatures2<vk::PhysicalDeviceFeatures2, vk::PhysicalDeviceVulkan12Features>();
        return features12.get<vk::PhysicalDeviceVulkan12Features>().timelineSemaphore == VK_TRUE;
    }

    if (feature == "shaderFloat16")
    {
        auto features12 = device.getFeatures2<vk::PhysicalDeviceFeatures2, vk::PhysicalDeviceVulkan12Features>();
        return features12.get<vk::PhysicalDeviceVulkan12Features>().shaderFloat16 == VK_TRUE;
    }

    if (feature == "shaderInt8")
    {
        auto features12 = device.getFeatures2<vk::PhysicalDeviceFeatures2, vk::PhysicalDeviceVulkan12Features>();
        return features12.get<vk::PhysicalDeviceVulkan12Features>().shaderInt8 == VK_TRUE;
    }

    // Vulkan 1.1 特性检查
    if (feature == "multiview")
    {
        auto features11 = device.getFeatures2<vk::PhysicalDeviceFeatures2, vk::PhysicalDeviceVulkan11Features>();
        return features11.get<vk::PhysicalDeviceVulkan11Features>().multiview == VK_TRUE;
    }

    if (feature == "variablePointers")
    {
        auto features11 = device.getFeatures2<vk::PhysicalDeviceFeatures2, vk::PhysicalDeviceVulkan11Features>();
        return features11.get<vk::PhysicalDeviceVulkan11Features>().variablePointers == VK_TRUE;
    }

    if (feature == "variablePointersStorageBuffer")
    {
        auto features11 = device.getFeatures2<vk::PhysicalDeviceFeatures2, vk::PhysicalDeviceVulkan11Features>();
        return features11.get<vk::PhysicalDeviceVulkan11Features>().variablePointersStorageBuffer == VK_TRUE;
    }

    // 基础特性检查（Vulkan 1.0）
    if (feature == "geometryShader")
    {
        auto features = device.getFeatures();
        return features.geometryShader == VK_TRUE;
    }

    if (feature == "tessellationShader")
    {
        auto features = device.getFeatures();
        return features.tessellationShader == VK_TRUE;
    }

    if (feature == "samplerAnisotropy")
    {
        auto features = device.getFeatures();
        return features.samplerAnisotropy == VK_TRUE;
    }

    if (feature == "fillModeNonSolid")
    {
        auto features = device.getFeatures();
        return features.fillModeNonSolid == VK_TRUE;
    }

    if (feature == "wideLines")
    {
        auto features = device.getFeatures();
        return features.wideLines == VK_TRUE;
    }

    if (feature == "largePoints")
    {
        auto features = device.getFeatures();
        return features.largePoints == VK_TRUE;
    }

    if (feature == "multiDrawIndirect")
    {
        auto features = device.getFeatures();
        return features.multiDrawIndirect == VK_TRUE;
    }

    if (feature == "drawIndirectFirstInstance")
    {
        auto features = device.getFeatures();
        return features.drawIndirectFirstInstance == VK_TRUE;
    }

    if (feature == "depthClamp")
    {
        auto features = device.getFeatures();
        return features.depthClamp == VK_TRUE;
    }

    if (feature == "depthBiasClamp")
    {
        auto features = device.getFeatures();
        return features.depthBiasClamp == VK_TRUE;
    }

    if (feature == "fragmentStoresAndAtomics")
    {
        auto features = device.getFeatures();
        return features.fragmentStoresAndAtomics == VK_TRUE;
    }

    if (feature == "shaderStorageImageExtendedFormats")
    {
        auto features = device.getFeatures();
        return features.shaderStorageImageExtendedFormats == VK_TRUE;
    }

    if (feature == "shaderStorageImageMultisample")
    {
        auto features = device.getFeatures();
        return features.shaderStorageImageMultisample == VK_TRUE;
    }

    if (feature == "shaderStorageImageReadWithoutFormat")
    {
        auto features = device.getFeatures();
        return features.shaderStorageImageReadWithoutFormat == VK_TRUE;
    }

    if (feature == "shaderStorageImageWriteWithoutFormat")
    {
        auto features = device.getFeatures();
        return features.shaderStorageImageWriteWithoutFormat == VK_TRUE;
    }

    if (feature == "shaderUniformBufferArrayDynamicIndexing")
    {
        auto features = device.getFeatures();
        return features.shaderUniformBufferArrayDynamicIndexing == VK_TRUE;
    }

    if (feature == "shaderSampledImageArrayDynamicIndexing")
    {
        auto features = device.getFeatures();
        return features.shaderSampledImageArrayDynamicIndexing == VK_TRUE;
    }

    if (feature == "shaderStorageBufferArrayDynamicIndexing")
    {
        auto features = device.getFeatures();
        return features.shaderStorageBufferArrayDynamicIndexing == VK_TRUE;
    }

    if (feature == "shaderStorageImageArrayDynamicIndexing")
    {
        auto features = device.getFeatures();
        return features.shaderStorageImageArrayDynamicIndexing == VK_TRUE;
    }

    if (feature == "shaderClipDistance")
    {
        auto features = device.getFeatures();
        return features.shaderClipDistance == VK_TRUE;
    }

    if (feature == "shaderCullDistance")
    {
        auto features = device.getFeatures();
        return features.shaderCullDistance == VK_TRUE;
    }

    if (feature == "shaderFloat64")
    {
        auto features = device.getFeatures();
        return features.shaderFloat64 == VK_TRUE;
    }

    if (feature == "shaderInt64")
    {
        auto features = device.getFeatures();
        return features.shaderInt64 == VK_TRUE;
    }

    if (feature == "shaderInt16")
    {
        auto features = device.getFeatures();
        return features.shaderInt16 == VK_TRUE;
    }

    if (feature == "shaderResourceResidency")
    {
        auto features = device.getFeatures();
        return features.shaderResourceResidency == VK_TRUE;
    }

    if (feature == "shaderResourceMinLod")
    {
        auto features = device.getFeatures();
        return features.shaderResourceMinLod == VK_TRUE;
    }

    if (feature == "sparseBinding")
    {
        auto features = device.getFeatures();
        return features.sparseBinding == VK_TRUE;
    }

    if (feature == "sparseResidencyBuffer")
    {
        auto features = device.getFeatures();
        return features.sparseResidencyBuffer == VK_TRUE;
    }

    if (feature == "sparseResidencyImage2D")
    {
        auto features = device.getFeatures();
        return features.sparseResidencyImage2D == VK_TRUE;
    }

    if (feature == "sparseResidencyImage3D")
    {
        auto features = device.getFeatures();
        return features.sparseResidencyImage3D == VK_TRUE;
    }

    if (feature == "sparseResidency2Samples")
    {
        auto features = device.getFeatures();
        return features.sparseResidency2Samples == VK_TRUE;
    }

    if (feature == "sparseResidency4Samples")
    {
        auto features = device.getFeatures();
        return features.sparseResidency4Samples == VK_TRUE;
    }

    if (feature == "sparseResidency8Samples")
    {
        auto features = device.getFeatures();
        return features.sparseResidency8Samples == VK_TRUE;
    }

    if (feature == "sparseResidency16Samples")
    {
        auto features = device.getFeatures();
        return features.sparseResidency16Samples == VK_TRUE;
    }

    if (feature == "sparseResidencyAliased")
    {
        auto features = device.getFeatures();
        return features.sparseResidencyAliased == VK_TRUE;
    }

    if (feature == "variableMultisampleRate")
    {
        auto features = device.getFeatures();
        return features.variableMultisampleRate == VK_TRUE;
    }

    if (feature == "inheritedQueries")
    {
        auto features = device.getFeatures();
        return features.inheritedQueries == VK_TRUE;
    }

    // 未识别的特性名称
    return false;
}

int Device::ratedevicescore(vk::PhysicalDevice &device)
{
    //暂时只区分离散GPU和集成GPU
    int score = 0;

    vk::PhysicalDeviceProperties deviceProperties = device.getProperties();
    vk::PhysicalDeviceFeatures deviceFeatures = device.getFeatures();

    // 优先选择离散GPU
    if (deviceProperties.deviceType == vk::PhysicalDeviceType::eDiscreteGpu)
    {
        score += 1000;
    }

    return score;
}

Device::~Device()
{
    cleanup();
}

void Device::cleanup()
{
    if (m_graphicsQueue)
    {
        m_graphicsQueue = nullptr;
    }

    if (m_device)
    {
        m_device.waitIdle();
        m_device.destroy();
        m_device = nullptr;
    }
}

} // namespace vkcore
