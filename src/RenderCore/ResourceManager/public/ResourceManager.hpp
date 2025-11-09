#pragma once

#include "RenderCore/VulkanCore/public/Device.hpp" // 包含 vkcore::Device
#include "ResourceManagerUtils.hpp"
#include "ResourceType.hpp"
#include <filesystem>
#include <future>
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

// 前向声明来自 ResourceManagerUtils.hpp 的加载器
namespace rendercore
{
struct TextureData;
struct MeshData; // 从文件加载的网格原始数据结构
} // namespace rendercore

namespace rendercore
{

/**
 * @class ResourceManager
 * @brief 负责加载、缓存和管理所有GPU资源 (Mesh, Texture, Material)
 * @details 这是一个核心服务，编排 VulkanCore 组件
 * 来执行数据驱动的资源加载。
 * 优化点：
 * 1. 封装加载与上传：loadMesh/loadTexture/loadMaterial
 * 现在一步到位，返回完全准备好的 GPU 资源。
 * 2. 简化的 API：移除了 createBufferFromMesh 等两步加载函数。
 * 3. 明确的注册接口：registerMesh/registerTexture
 * 现在接受 CPU 数据（顶点/像素）并自动处理上传。
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

    // ==================== 资源加载接口 (Get-or-Load) ====================

    /**
     * @brief 加载或获取缓存的网格 (例如 .obj)
     * @details 自动处理文件加载、解析和GPU缓冲区创建/上传
     * @param filepath 文件路径 (用作缓存键)
     * @return std::shared_ptr<Mesh> GPU 就绪的网格资源
     */
    std::shared_ptr<Mesh> loadMesh(const std::filesystem::path &filepath);

    /**
     * @brief 加载或获取缓存的纹理
     * @details 自动处理文件加载、解析和GPU图像创建/上传
     * @param filepath 文件路径 (用作缓存键)
     * @param srgb 纹理是否为 sRGB 格式
     * @return std::shared_ptr<Texture> GPU 就绪的纹理资源
     */
    std::shared_ptr<Texture> loadTexture(const std::filesystem::path &filepath, bool srgb = false);

    /**
     * @brief 加载或获取缓存的材质 (通过 .json 文件定义)
     * @details 自动加载JSON，递归加载其纹理和着色器，
     * 并创建和更新 DescriptorSet。
     * @param filepath 材质定义文件 (.json) 的路径
     * @return std::shared_ptr<Material> GPU 就绪的材质资源
     */
    std::shared_ptr<Material> loadMaterial(const std::filesystem::path &filepath);

    // ==================== 异步加载接口 (可选) ====================

    /**
     * @brief 异步加载网格
     * @param filepath 文件路径
     * @return std::future<std::shared_ptr<Mesh>>
     */
    std::future<std::shared_ptr<Mesh>> loadMeshAsync(const std::filesystem::path &filepath);

    /**
     * @brief 异步加载纹理
     * @param filepath 文件路径
     * @param srgb 是否为 sRGB
     * @return std::future<std::shared_ptr<Texture>>
     */
    std::future<std::shared_ptr<Texture>> loadTextureAsync(const std::filesystem::path &filepath, bool srgb = false);

    // ==================== 资源注册接口 (程序化创建) ====================

    /**
     * @brief 注册自定义网格（从原始顶点/索引数据）
     * @param name 网格的唯一名称
     * @param vertices 顶点数据
     * @param indices 索引数据
     * @return std::shared_ptr<Mesh> GPU 就绪的网格资源
     */
    std::shared_ptr<Mesh> registerMesh(const std::string &name, const std::vector<Vertex> &vertices,
                                       const std::vector<uint32_t> &indices);

    /**
     * @brief 注册自定义纹理（从原始像素数据）
     * @param name 纹理的唯一名称
     * @param pixels 像素数据
     * @param width 宽度
     * @param height 高度
     * @param format 格式
     * @return std::shared_ptr<Texture> GPU 就绪的纹理资源
     */
    std::shared_ptr<Texture> registerTexture(const std::string &name, const void *pixels, int width, int height,
                                             vk::Format format);

    /**
     * @brief 注册自定义材质
     * @param name 材质的唯一名称
     * @param materialInfo 材质参数
     * @param textureNames 依赖的纹理名称 (必须已通过 loadTexture/registerTexture 注册)
     * @param shaderName 依赖的着色器名称 (必须已在 ShaderManager 中)
     * @return std::shared_ptr<Material> GPU 就绪的材质资源
     */
    std::shared_ptr<Material> registerMaterial(const std::string &name, const Material &materialInfo,
                                               const TexturePaths &textureNames, const std::string &shaderName);

    // ==================== 资源访问与管理 ====================

    /**
     * @brief 通过名称获取已缓存的网格
     */
    std::shared_ptr<Mesh> getMesh(const std::string &name) const;

    /**
     * @brief 通过名称获取已缓存的纹理
     */
    std::shared_ptr<Texture> getTexture(const std::string &name) const;

    /**
     * @brief 通过名称获取已缓存的材质
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

    /**
     * @brief 卸载指定的网格资源
     */
    bool unloadMesh(const std::string &name);

    /**
     * @brief 卸载指定的纹理资源
     */
    bool unloadTexture(const std::string &name);

    /**
     * @brief 卸载指定的材质资源
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

    // ==================== 描述符布局访问接口 ====================

    /**
     * @brief 获取材质描述符集布局
     * @details 用于在管线创建时添加到PipelineBuilder中
     * @return vk::DescriptorSetLayout 材质描述符集布局句柄
     */
    vk::DescriptorSetLayout getMaterialLayout() const;

    /**
     * @brief 检查ResourceManager是否已初始化
     * @return bool 是否已初始化
     */
    bool isInitialized() const;

  private:
    // ==================== 私有辅助函数 (all_lowercase) ====================

    /**
     * @brief (私有) 为所有PBR材质创建标准的描述符集布局
     * @details 在 initialize() 中调用，创建并缓存一个布局
     */
    void buildmateriallayout();

    /**
     * @brief (私有) 创建并上传一个 vkcore::Buffer
     */
    std::shared_ptr<vkcore::Buffer> createbufferfromdata(const void *data, vk::DeviceSize size,
                                                         vk::BufferUsageFlags usage);

    /**
     * @brief (私有) 创建并上传一个 vkcore::Image
     */
    std::shared_ptr<vkcore::Image> createimagefromdata(const void *data, int width, int height, vk::Format format);

    /**
     * @brief (私有) 创建所有默认纹理 (1x1 白色, 1x1 法线)
     */
    void createdefaulttextures();

    /**
     * @brief (私有) 为材质创建Uniform Buffer
     */
    void createMaterialUniformBuffer(std::shared_ptr<Material> material);

    /**
     * @brief (私有) 更新材质的描述符集，绑定纹理和采样器
     */
    void updateMaterialDescriptorSet(std::shared_ptr<Material> material);

    /**
     * @brief (私有) 获取或创建缓存的采样器
     */
    vk::Sampler getorsampler(vk::Filter filter, vk::SamplerAddressMode addressMode);

    /**
     * @brief (私有) loadMaterial 和 registerMaterial 的通用逻辑
     * @details 负责链接纹理、着色器并创建 DescriptorSet
     */
    std::shared_ptr<Material> buildmaterial(const std::string &name, const Material &materialInfo,
                                            const TexturePaths &textureNames, const std::string &shaderName);

    /**
     * @brief (私有) 实际的纹理加载和上传逻辑
     */
    std::shared_ptr<Texture> loadanduploadtexture(const std::filesystem::path &filepath, bool srgb);

    /**
     * @brief (私有) 实际的网格加载和上传逻辑
     */
    std::shared_ptr<Mesh> loadanduploadmesh(const std::filesystem::path &filepath);

    /**
     * @brief (私有) 合并多个网格数据为单一网格
     */
    MeshData mergeMeshData(const std::vector<MeshData> &meshDataList, const std::string &baseName);

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

    // 资源缓存 (使用文件路径或注册名称作为键)
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
    mutable std::mutex m_mtx;
};

} // namespace rendercore