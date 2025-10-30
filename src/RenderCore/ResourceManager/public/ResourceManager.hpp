#pragma once

#include "../VulkanCore/public/Device.hpp" // 包含 vkcore::Device
#include "ResourceType.hpp"
#include <mutex>
#include <string>
#include <unordered_map>
#include <vma/vk_mem_alloc.h>

// 前向声明
namespace vkcore
{
class CommandPoolManager;
class ShaderManager;
class DescriptorAllocator;
class DescriptorLayoutCache;
} // namespace vkcore

namespace rendercore
{

/**
 * @class ResourceManager
 * @brief 负责加载、缓存和管理所有GPU资源 (Mesh, Texture, Material)
 * @details 这是一个核心服务，编排 VulkanCore 组件
 * 来执行数据驱动的资源加载。
 */
class ResourceManager
{
  public:
    ResourceManager() = default;
    ~ResourceManager();

    /** 禁用拷贝与移动 */
    ResourceManager(const ResourceManager &) = delete;
    ResourceManager &operator=(const ResourceManager &) = delete;

    /**
     * @brief 初始化管理器，提供所有必需的Vulkan上下文
     * @param device vkcore::Device 引用
     * @param allocator VmaAllocator 句柄
     * @param cmdManager 用于上传的命令池管理器
     * @param shaderManager 着色器缓存
     * @param descAllocator 描述符分配器
     * @param layoutCache 描述符布局缓存
     */
    void initialize(vkcore::Device &device, VmaAllocator allocator, vkcore::CommandPoolManager &cmdManager,
                    vkcore::ShaderManager &shaderManager, vkcore::DescriptorAllocator &descAllocator,
                    vkcore::DescriptorLayoutCache &layoutCache);

    /**
     * @brief 清理所有缓存的GPU资源
     */
    void cleanup();

    // ==================== 公共加载接口 (camelCase) ====================

    /**
     * @brief 加载或获取缓存的网格 (例如 .obj)
     * @param filepath 文件路径 (用作缓存键)
     * @return std::shared_ptr<Mesh> 网格资源
     */
    std::shared_ptr<Mesh> loadMesh(const std::string &filepath);

    /**
     * @brief 加载或获取缓存的纹理
     * @param filepath 文件路径 (用作缓存键)
     * @param srgb 纹理是否为 sRGB 格式
     * @return std::shared_ptr<Texture> 纹理资源
     */
    std::shared_ptr<Texture> loadTexture(const std::string &filepath, bool srgb);

    /**
     * @brief 加载或获取缓存的材质 (通过 .json 文件定义)
     * @param filepath 材质定义文件 (.json) 的路径
     * @return std::shared_ptr<Material> 材质资源
     */
    std::shared_ptr<Material> loadMaterial(const std::string &filepath);

    /**
     * @brief 获取默认的1x1白色纹理 (用于无纹理材质)
     */
    std::shared_ptr<Texture> getDefaultWhiteTexture();

  private:
    // ==================== 私有辅助函数 (all_lowercase) ====================

    /**
     * @brief (私有) 为所有PBR材质创建标准的描述符集布局
     * @details 在 initialize() 中调用，创建并缓存一个布局，
     * 例如: binding 0: baseColor, binding 1: normal, ...
     */
    void buildmateriallayout();

    /**
     * @brief (私有) 创建并上传一个 vkcore::Buffer
     * @param data 原始数据指针 (如顶点/索引)
     * @param size 数据大小 (字节)
     * @param usage 缓冲区用途 (VK_BUFFER_USAGE_VERTEX_BUFFER_BIT 等)
     * @return std::shared_ptr<vkcore::Buffer>
     */
    std::shared_ptr<vkcore::Buffer> createbufferfromdata(const void *data, vk::DeviceSize size,
                                                         vk::BufferUsageFlags usage);

    /**
     * @brief (私有) 创建并上传一个 vkcore::Image
     * @details 这是 main.cpp 中 createRedTexture 的泛化版本
     * @param data 图像的原始像素数据
     * @param width 图像宽度
     * @param height 图像高度
     * @param format 图像格式
     * @return std::shared_ptr<vkcore::Image>
     */
    std::shared_ptr<vkcore::Image> createimagefromdata(const void *data, int width, int height, vk::Format format);

    /**
     * @brief (私有) 创建所有默认纹理 (1x1 白色, 1x1 法线)
     */
    void createdefaulttextures();

    /**
     * @brief (私有) 获取或创建缓存的采样器
     */
    vk::Sampler getorsampler(vk::Filter filter, vk::SamplerAddressMode addressMode);

  private:
    // ==================== 私有成员 (m_ prefix, all_lowercase) ====================
    bool m_initialized = false;

    // Vulkan 上下文 (从外部传入的引用)
    vkcore::Device *m_device = nullptr;
    VmaAllocator m_allocator = nullptr;
    vkcore::CommandPoolManager *m_cmdManager = nullptr;
    vkcore::ShaderManager *m_shaderManager = nullptr;
    vkcore::DescriptorAllocator *m_descAllocator = nullptr;
    vkcore::DescriptorLayoutCache *m_layoutCache = nullptr;

    // 资源缓存 (使用文件路径作为键)
    std::unordered_map<std::string, std::shared_ptr<Mesh>> m_meshCache;
    std::unordered_map<std::string, std::shared_ptr<Texture>> m_textureCache;
    std::unordered_map<std::string, std::shared_ptr<Material>> m_materialCache;

    // 采样器缓存 (使用配置哈希作为键)
    std::unordered_map<uint64_t, vk::Sampler> m_samplerCache;

    // 默认资源
    std::shared_ptr<Texture> m_defaultWhiteTexture;
    std::shared_ptr<Texture> m_defaultNormalTexture;

    // 标准材质布局
    vk::DescriptorSetLayout m_materialLayout;

    // 互斥锁，保护所有缓存的线程安全
    std::mutex m_mtx;
};

} // namespace rendercore