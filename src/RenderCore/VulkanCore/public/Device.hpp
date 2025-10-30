/**
 * @file Device.hpp
 * @brief 封装 Vulkan 物理设备与逻辑设备的创建与管理的类声明。
 *
 * 提供设备查询、队列检索与清理接口。具体实现应在对应的 cpp 文件中完成。
 * 注意：该类不拥有 vk::Instance，仅持有引用，实例生命周期应先于 Device。
 */
#pragma once
#include <optional>
#include <string>
#include <vector>
#include <vulkan/vulkan.hpp>

namespace vkcore
{

/**
 * @class Device
 * @brief 封装 Vulkan 设备相关资源与操作逻辑。
 *
 * 主要职责：
 * - 查找并选择合适的物理设备（PhysicalDevice）
 * - 创建逻辑设备（vk::Device）
 * - 获取图形与呈现用队列（vk::Queue）
 * - 提供对实例与物理设备的访问接口
 */
class Device
{
  public:
    /**
     * @struct Config
     * @brief 设备创建的配置信息（目前仅包含需要启用的设备扩展列表）。
     *
     * @property deviceExtensions 所需启用的设备扩展名称数组（以 const char* 表示）。
     */
    struct Config
    {
        std::vector<std::string> deviceExtensions;
        std::vector<std::string> vulkan1_3_features;
        std::vector<std::string> vulkan1_2_features;
        std::vector<std::string> vulkan1_1_features;
    };

    /**
     * @struct QueueFamilyIndices
     * @brief 保存队列族索引（可能不存在时使用 std::optional）。
     *
     * isComplete() 用于判断是否同时支持图形与呈现队列。
     *
     * @property graphicsFamily 图形队列族索引
     * @property presentFamily  呈现队列族索引
     */
    struct QueueFamilyIndices
    {
        std::optional<uint32_t> graphicsFamily;
        std::optional<uint32_t> presentFamily;

        bool isComplete() const
        {
            return graphicsFamily.has_value() && presentFamily.has_value();
        }
    };

  public:
    /**
     * @brief 构造函数
     *
     * @constructor
     * @param instance Vulkan 实例引用（必须有效）
     * @param config 可选设备配置（例如所需扩展），默认为空配置
     *
     * 构造完成后将调用 init() 执行设备选择与创建流程。
     */
    Device(vk::Instance &instance, VkSurfaceKHR &surface, const Config &config = {});
    ~Device();

    /** 禁用拷贝与移动语义 */
    Device(const Device &) = delete;
    Device &operator=(const Device &) = delete;
    Device(Device &&) = delete;
    Device &operator=(Device &&) = delete;

    /**
     * @brief 返回逻辑设备句柄。
     * @return vk::Device 当前逻辑设备句柄。调用前请确保设备已初始化且未被 cleanup() 释放。
     */
    inline vk::Device get() const
    {
        return m_device;
    }

    /**
     * @brief 返回已选择的物理设备。
     * @return vk::PhysicalDevice 当前物理设备句柄。
     */
    inline vk::PhysicalDevice getPhysicalDevice() const
    {
        return m_physicalDevice;
    }

    /**
     * @brief 返回 Vulkan 实例的引用。
     * @return vk::Instance& Vulkan 实例引用。
     */
    inline vk::Instance &getInstance() const
    {
        return m_instance;
    }

    /**
     * @brief 返回图形队列句柄。
     * @return vk::Queue 图形队列。
     */
    inline vk::Queue getGraphicsQueue() const
    {
        return m_graphicsQueue;
    }

    /**
     * @brief 返回呈现队列句柄。
     * @return vk::Queue 呈现队列。
     */
    inline vk::Queue getPresentQueue() const
    {
        return m_presentQueue;
    }

    /**
     * @brief 返回图形队列族索引信息。
     * @return QueueFamilyIndices 队列族索引结构体。
     */
    inline uint32_t getGraphicsQueueFamilyIndices() const
    {
        return m_queueFamilyIndices.graphicsFamily.value();
    }

    /**
     * @brief 返回呈现队列族索引信息。
     * @return QueueFamilyIndices 队列族索引结构体。
     */
    inline uint32_t getPresentQueueFamilyIndices() const
    {
        return m_queueFamilyIndices.presentFamily.value();
    }

    /**
     * @brief 释放由 Device 创建的资源（如逻辑设备），并进行必要的清理。
     *
     * 在析构前或显式需要时调用。
     */
    void cleanup();

  private:
    /**
     * @brief 逻辑设备句柄：用于记录与提交命令、创建资源等运行时操作。
     *
     * 在初始化完成后有效，Cleanup() 将负责释放。
     */
    vk::Device m_device;

    /**
     * @brief 对 Vulkan 实例的引用，Device 创建与查询会使用实例信息（扩展、层等）。
     *
     * 注意：类不拥有实例，仅持有引用，实例生命周期应先于 Device。
     */
    vk::Instance &m_instance;

    /**
     * @brief Vulkan 表面句柄，用于呈现相关操作（如交换链创建）。
     * 注意：类不拥有表面，仅持有引用，表面生命周期应先于 Device。
     */
    VkSurfaceKHR &m_surface;

    /**
     * @brief 被选择的物理设备（GPU）。用于查询特性、队列族与能力。
     */
    vk::PhysicalDevice m_physicalDevice;

    /**
     * @brief 图形队列句柄：用于提交图形命令。
     */
    vk::Queue m_graphicsQueue;

    /**
     * @brief 呈现队列句柄：用于提交用于展示（present）的命令。
     *
     * 可能与图形队列相同，也可能不同，取决于队列族。
     */
    vk::Queue m_presentQueue;

    /**
     * @brief 设备配置信息，用于检查物理设备特性
     */
    Config m_config;

    /**
     * @brief 队列族索引信息
     */
    QueueFamilyIndices m_queueFamilyIndices;

  private:
    /**
     * @brief 遍历实例上的物理设备，挑选满足需求的设备（扩展、队列支持等）。
     *
     * 成功后设置 m_physicalDevice。
     */
    void selectphyscialdevice();

    /**
     * @brief 查询并记录所选物理设备的队列族索引（图形/呈现）。
     *
     * 设置 QueueFamilyIndices 中的值以便创建逻辑设备时使用。
     */
    void findqueuefamilies();

    /**
     * @brief 基于选定的物理设备与所需队列族创建 vk::Device（逻辑设备），并获取队列句柄。
     *
     * 使用 Config 中配置的设备扩展。
     */
    void createlogicaldevice();

    /**
     * @brief 检查物理设备是否支持所需的设备扩展
     * @param device 要检查的物理设备
     * @return true 如果支持所有必需的扩展
     */
    bool checkdeviceextensionsupport(vk::PhysicalDevice &device);

    /**
     * @brief 检查物理设备是否支持所需的Vulkan特性
     * @param device 要检查的物理设备
     * @return true 如果支持所有必需的特性
     */
    bool checkvulkanfeaturessupport(vk::PhysicalDevice &device);

    /**
     * @brief 检查物理设备是否满足基本特性要求
     * @param device 要检查的物理设备
     * @return true 如果满足基本特性要求
     */
    bool checkspeficfeaturesupport(vk::PhysicalDevice &device, std::string feature);

    /**
     * @brief 评价物理设备的适用性，返回评分值
     * @param device 要检查的物理设备
     * @return int 评分值，值越高表示设备越适合使用
     */
    int ratedevicescore(vk::PhysicalDevice &device);
};

} // namespace vkcore
