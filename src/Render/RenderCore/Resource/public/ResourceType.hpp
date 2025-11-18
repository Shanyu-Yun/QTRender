#pragma once

#include "RenderCore/VulkanCore/public/Descriptor.hpp"    // 包含 vkcore::Descriptor...
#include "RenderCore/VulkanCore/public/ShaderManager.hpp" // 包含 vkcore::ShaderModule
#include "RenderCore/VulkanCore/public/VKResource.hpp"    // 包含 vkcore::Buffer 和 vkcore::Image
#include <glm/glm.hpp>
#include <memory>
#include <string>

namespace rendercore
{

/**
 * @struct Vertex
 * @brief 顶点的标准布局
 * @details 成员按大小降序排列以优化内存对齐
 */
struct Vertex
{
    glm::vec4 color;    // 16 bytes
    glm::vec3 position; // 12 bytes
    glm::vec3 normal;   // 12 bytes
    glm::vec2 texCoord; // 8 bytes
};

/**
 * @struct Mesh
 * @brief 包含顶点和索引缓冲区的网格资源
 */
struct Mesh
{
    std::string name; ///< 网格名称（用于调试和资源管理）
    std::shared_ptr<vkcore::Buffer> vertexBuffer;
    std::shared_ptr<vkcore::Buffer> indexBuffer;
    uint32_t vertexCount{0}; ///< 顶点数量（用于无索引绘制）
    uint32_t indexCount{0};  ///< 索引数量
    // (未来可以添加包围盒等)
};

/**
 * @struct Texture
 * @brief 包含图像、视图和采样器的纹理资源
 * @details Image 和 Sampler 是分离的，以提高灵活性
 */
struct Texture
{
    std::string name; ///< 纹理名称（用于调试和资源管理）
    std::shared_ptr<vkcore::Image> image;
    vk::UniqueSampler sampler; ///< RAII 管理的采样器（自动销毁）
};

/**
 * @enum AlphaMode
 * @brief Alpha 混合模式
 */
enum class AlphaMode
{
    Opaque, ///< 不透明（忽略 alpha 值）
    Mask,   ///< Alpha 测试（alphaCutoff 阈值）
    Blend   ///< Alpha 混合（透明）
};

/**
 * @struct Material
 * @brief 材质，定义了渲染一个表面所需的所有参数和资源
 */
struct Material
{
    std::string name; ///< 材质名称（用于调试和资源管理）

    // PBR 金属-粗糙度 工作流参数
    glm::vec4 baseColorFactor{1.0f, 1.0f, 1.0f, 1.0f};
    glm::vec3 emissiveFactor{0.0f, 0.0f, 0.0f}; ///< 自发光因子
    float metallicFactor{1.0f};
    float roughnessFactor{1.0f};
    float alphaCutoff{0.5f}; ///< Alpha 测试阈值（仅在 AlphaMode::Mask 时使用）

    // 扩展参数
    float normalScale{1.0f};     ///< 法线强度
    float refractionIndex{1.0f}; ///< 折射率（用于透明材质）

    // 渲染状态
    AlphaMode alphaMode{AlphaMode::Opaque}; ///< Alpha 混合模式
    bool doubleSided{false};                ///< 是否双面渲染

    // 纹理 (由 ResourceManager 管理) - 支持分离的金属度/粗糙度纹理
    std::shared_ptr<Texture> baseColorTexture;
    std::shared_ptr<Texture> metallicTexture;  ///< 独立金属度纹理
    std::shared_ptr<Texture> roughnessTexture; ///< 独立粗糙度纹理
    std::shared_ptr<Texture> normalTexture;
    std::shared_ptr<Texture> occlusionTexture; ///< AO 纹理
    std::shared_ptr<Texture> emissiveTexture;

    // 着色器 (由 ShaderManager 管理)
    std::shared_ptr<vkcore::ShaderModule> vertexShader;
    std::shared_ptr<vkcore::ShaderModule> fragmentShader;

    // GPU资源
    std::shared_ptr<vkcore::Buffer> uniformBuffer; ///< 材质参数Uniform Buffer

    // Vulkan 描述符集 (Material 的核心 "实例")
    // 当一个 Material 被创建时，它应该被分配一个描述符集
    vk::DescriptorSet descriptorSet;
};

} // namespace rendercore