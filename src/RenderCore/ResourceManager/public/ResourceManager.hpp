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
    std::shared_ptr<Texture> loadTexture(const std::string &filepath, bool srgb = false);

    /**
     * @brief 加载或获取缓存的材质 (通过 .json 文件定义)
     * @param filepath 材质定义文件 (.json) 的路径
     * @return std::shared_ptr<Material> 材质资源
     */
    std::shared_ptr<Material> loadMaterial(const std::string &filepath);

    /**
     * @brief 通过名称获取已缓存的网格
     * @param name 网格名称或文件路径
     * @return std::shared_ptr<Mesh> 网格资源，如果不存在则返回 nullptr
     */
    std::shared_ptr<Mesh> getMesh(const std::string &name) const;

    /**
     * @brief 通过名称获取已缓存的纹理
     * @param name 纹理名称或文件路径
     * @return std::shared_ptr<Texture> 纹理资源，如果不存在则返回 nullptr
     */
    std::shared_ptr<Texture> getTexture(const std::string &name) const;

    /**
     * @brief 通过名称获取已缓存的材质
     * @param name 材质名称或文件路径
     * @return std::shared_ptr<Material> 材质资源，如果不存在则返回 nullptr
     */
    std::shared_ptr<Material> getMaterial(const std::string &name) const;

    /**
     * @brief 获取默认的1x1白色纹理 (用于无纹理材质)
     */
    std::shared_ptr<Texture> getDefaultWhiteTexture();

    /**
     * @brief 获取默认的法线贴图 (用于无法线贴图的材质)
     */
    std::shared_ptr<Texture> getDefaultNormalTexture();

    // ==================== GPU 缓冲区/图像创建接口 ====================

    /**
     * @brief 从已加载的网格创建 GPU 缓冲区
     * @param meshName 网格名称或文件路径
     * @return true 如果成功创建缓冲区，false 如果网格不存在或已有缓冲区
     */
    bool createBufferFromMesh(const std::string &meshName);

    /**
     * @brief 从已加载的纹理创建 GPU 图像
     * @param textureName 纹理名称或文件路径
     * @return true 如果成功创建图像，false 如果纹理不存在或已有图像
     */
    bool createImageFromTexture(const std::string &textureName);

    /**
     * @brief 从已加载的材质创建 GPU 资源（纹理图像 + 描述符集）
     * @param materialName 材质名称或文件路径
     * @return true 如果成功创建资源，false 如果材质不存在或已有资源
     */
    bool createResourcesFromMaterial(const std::string &materialName);

    /**
     * @brief 批量创建多个网格的 GPU 缓冲区
     * @param meshNames 网格名称列表
     * @return 成功创建的数量
     */
    size_t createBuffersFromMeshes(const std::vector<std::string> &meshNames);

    /**
     * @brief 批量创建多个纹理的 GPU 图像
     * @param textureNames 纹理名称列表
     * @return 成功创建的数量
     */
    size_t createImagesFromTextures(const std::vector<std::string> &textureNames);

    /**
     * @brief 注册自定义网格（不从文件加载）
     * @param name 网格的唯一名称
     * @param mesh 网格数据
     * @return true 如果成功注册，false 如果名称已存在
     */
    bool registerMesh(const std::string &name, std::shared_ptr<Mesh> mesh);

    /**
     * @brief 注册自定义纹理（不从文件加载）
     * @param name 纹理的唯一名称
     * @param texture 纹理数据
     * @return true 如果成功注册，false 如果名称已存在
     */
    bool registerTexture(const std::string &name, std::shared_ptr<Texture> texture);

    /**
     * @brief 注册自定义材质（不从文件加载）
     * @param name 材质的唯一名称
     * @param material 材质数据
     * @return true 如果成功注册，false 如果名称已存在
     */
    bool registerMaterial(const std::string &name, std::shared_ptr<Material> material);

    /**
     * @brief 卸载指定的网格资源
     * @param name 网格名称
     * @return true 如果成功卸载，false 如果不存在
     */
    bool unloadMesh(const std::string &name);

    /**
     * @brief 卸载指定的纹理资源
     * @param name 纹理名称
     * @return true 如果成功卸载，false 如果不存在
     */
    bool unloadTexture(const std::string &name);

    /**
     * @brief 卸载指定的材质资源
     * @param name 材质名称
     * @return true 如果成功卸载，false 如果不存在
     */
    bool unloadMaterial(const std::string &name);

    /**
     * @brief 获取所有已缓存的网格名称
     */
    std::vector<std::string> getMeshNames() const;

    /**
     * @brief 获取所有已缓存的纹理名称
     */
    std::vector<std::string> getTextureNames() const;

    /**
     * @brief 获取所有已缓存的材质名称
     */
    std::vector<std::string> getMaterialNames() const;

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