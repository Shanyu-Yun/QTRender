#include "ResourceManager.hpp"
#include "RenderCore/VulkanCore/public/CommandPoolManager.hpp"
#include "RenderCore/VulkanCore/public/Descriptor.hpp"
#include "RenderCore/VulkanCore/public/ShaderManager.hpp"
#include <functional>
#include <iostream>
#include <stb_image.h>
#include <stdexcept>

namespace rendercore
{

ResourceManager::~ResourceManager()
{
    cleanup();
}

void ResourceManager::initialize(vkcore::Device &device, VmaAllocator allocator, vkcore::CommandPoolManager &cmdManager,
                                 vkcore::ShaderManager &shaderManager, vkcore::DescriptorAllocator &descAllocator,
                                 vkcore::DescriptorLayoutCache &layoutCache)
{
    std::lock_guard<std::mutex> lock(m_mtx);

    m_device = &device;
    m_allocator = allocator;
    m_cmdManager = &cmdManager;
    m_shaderManager = &shaderManager;
    m_descAllocator = &descAllocator;
    m_layoutCache = &layoutCache;

    buildmateriallayout();
    createdefaulttextures();

    m_initialized = true;
}

void ResourceManager::cleanup()
{
    std::lock_guard<std::mutex> lock(m_mtx);

    if (!m_initialized)
        return;

    // 清理缓存的资源
    m_meshCache.clear();
    m_textureCache.clear();
    m_materialCache.clear();

    // 清理采样器缓存
    for (auto &pair : m_samplerCache)
    {
        m_device->get().destroySampler(pair.second);
    }
    m_samplerCache.clear();

    // 清理描述符集布局
    if (m_materialLayout)
    {
        m_device->get().destroyDescriptorSetLayout(m_materialLayout);
        m_materialLayout = nullptr;
    }

    // 清理默认资源
    m_defaultWhiteTexture.reset();
    m_defaultNormalTexture.reset();

    m_initialized = false;
}

// ==================== 资源加载接口 ====================

std::shared_ptr<Mesh> ResourceManager::loadMesh(const std::filesystem::path &filepath)
{
    std::lock_guard<std::mutex> lock(m_mtx);

    if (!m_initialized)
    {
        throw std::runtime_error("ResourceManager not initialized");
    }

    std::string key = filepath.string();

    // 检查缓存
    auto it = m_meshCache.find(key);
    if (it != m_meshCache.end())
    {
        return it->second;
    }

    // 加载并缓存
    auto mesh = loadanduploadmesh(filepath);
    m_meshCache[key] = mesh;
    return mesh;
}

std::shared_ptr<Texture> ResourceManager::loadTexture(const std::filesystem::path &filepath, bool srgb)
{
    std::lock_guard<std::mutex> lock(m_mtx);

    if (!m_initialized)
    {
        throw std::runtime_error("ResourceManager not initialized");
    }

    std::string key = filepath.string() + (srgb ? "_srgb" : "_linear");

    // 检查缓存
    auto it = m_textureCache.find(key);
    if (it != m_textureCache.end())
    {
        return it->second;
    }

    // 加载并缓存
    auto texture = loadanduploadtexture(filepath, srgb);
    m_textureCache[key] = texture;
    return texture;
}

std::shared_ptr<Material> ResourceManager::loadMaterial(const std::filesystem::path &filepath)
{
    std::lock_guard<std::mutex> lock(m_mtx);

    if (!m_initialized)
    {
        throw std::runtime_error("ResourceManager not initialized");
    }

    std::string key = filepath.string();

    // 检查缓存
    auto it = m_materialCache.find(key);
    if (it != m_materialCache.end())
    {
        return it->second;
    }

    // 从JSON加载材质数据
    MaterialData materialData = MaterialLoader::loadMaterialData(filepath);

    // 构建材质
    auto material = buildmaterial(key, materialData.material, materialData.texturePaths, materialData.shaderName);

    m_materialCache[key] = material;
    return material;
}

// ==================== 异步加载接口 ====================

std::future<std::shared_ptr<Mesh>> ResourceManager::loadMeshAsync(const std::filesystem::path &filepath)
{
    return std::async(std::launch::async, [this, filepath]() { return loadMesh(filepath); });
}

std::future<std::shared_ptr<Texture>> ResourceManager::loadTextureAsync(const std::filesystem::path &filepath,
                                                                        bool srgb)
{
    return std::async(std::launch::async, [this, filepath, srgb]() { return loadTexture(filepath, srgb); });
}

// ==================== 资源注册接口 ====================

std::shared_ptr<Mesh> ResourceManager::registerMesh(const std::string &name, const std::vector<Vertex> &vertices,
                                                    const std::vector<uint32_t> &indices)
{
    std::lock_guard<std::mutex> lock(m_mtx);

    if (!m_initialized)
    {
        throw std::runtime_error("ResourceManager not initialized");
    }

    // 检查是否已存在
    auto it = m_meshCache.find(name);
    if (it != m_meshCache.end())
    {
        return it->second;
    }

    // 创建网格
    auto mesh = std::make_shared<Mesh>();
    mesh->name = name;
    mesh->vertexCount = static_cast<uint32_t>(vertices.size());
    mesh->indexCount = static_cast<uint32_t>(indices.size());

    // 创建顶点缓冲区
    if (!vertices.empty())
    {
        mesh->vertexBuffer = createbufferfromdata(vertices.data(), vertices.size() * sizeof(Vertex),
                                                  vk::BufferUsageFlagBits::eVertexBuffer);
    }

    // 创建索引缓冲区
    if (!indices.empty())
    {
        mesh->indexBuffer = createbufferfromdata(indices.data(), indices.size() * sizeof(uint32_t),
                                                 vk::BufferUsageFlagBits::eIndexBuffer);
    }

    m_meshCache[name] = mesh;
    return mesh;
}

std::shared_ptr<Texture> ResourceManager::registerTexture(const std::string &name, const void *pixels, int width,
                                                          int height, vk::Format format)
{
    std::lock_guard<std::mutex> lock(m_mtx);

    if (!m_initialized)
    {
        throw std::runtime_error("ResourceManager not initialized");
    }

    // 检查是否已存在
    auto it = m_textureCache.find(name);
    if (it != m_textureCache.end())
    {
        return it->second;
    }

    // 创建纹理
    auto texture = std::make_shared<Texture>();
    texture->name = name;

    // 创建图像
    texture->image = createimagefromdata(pixels, width, height, format);

    // 创建采样器
    vk::SamplerCreateInfo samplerInfo = {};
    samplerInfo.magFilter = vk::Filter::eLinear;
    samplerInfo.minFilter = vk::Filter::eLinear;
    samplerInfo.addressModeU = vk::SamplerAddressMode::eRepeat;
    samplerInfo.addressModeV = vk::SamplerAddressMode::eRepeat;
    samplerInfo.addressModeW = vk::SamplerAddressMode::eRepeat;
    samplerInfo.anisotropyEnable = VK_TRUE;
    samplerInfo.maxAnisotropy = 16.0f;
    samplerInfo.borderColor = vk::BorderColor::eIntOpaqueBlack;
    samplerInfo.unnormalizedCoordinates = VK_FALSE;
    samplerInfo.compareEnable = VK_FALSE;
    samplerInfo.compareOp = vk::CompareOp::eAlways;
    samplerInfo.mipmapMode = vk::SamplerMipmapMode::eLinear;

    texture->sampler = m_device->get().createSamplerUnique(samplerInfo);

    m_textureCache[name] = texture;
    return texture;
}

std::shared_ptr<Material> ResourceManager::registerMaterial(const std::string &name, const Material &materialInfo,
                                                            const TexturePaths &textureNames,
                                                            const std::string &shaderName)
{
    std::lock_guard<std::mutex> lock(m_mtx);

    if (!m_initialized)
    {
        throw std::runtime_error("ResourceManager not initialized");
    }

    // 检查是否已存在
    auto it = m_materialCache.find(name);
    if (it != m_materialCache.end())
    {
        return it->second;
    }

    auto material = buildmaterial(name, materialInfo, textureNames, shaderName);
    m_materialCache[name] = material;
    return material;
}

// ==================== 资源访问与管理 ====================

std::shared_ptr<Mesh> ResourceManager::getMesh(const std::string &name) const
{
    std::lock_guard<std::mutex> lock(m_mtx);

    auto it = m_meshCache.find(name);
    return (it != m_meshCache.end()) ? it->second : nullptr;
}

std::shared_ptr<Texture> ResourceManager::getTexture(const std::string &name) const
{
    std::lock_guard<std::mutex> lock(m_mtx);

    auto it = m_textureCache.find(name);
    return (it != m_textureCache.end()) ? it->second : nullptr;
}

std::shared_ptr<Material> ResourceManager::getMaterial(const std::string &name) const
{
    std::lock_guard<std::mutex> lock(m_mtx);

    auto it = m_materialCache.find(name);
    return (it != m_materialCache.end()) ? it->second : nullptr;
}

std::shared_ptr<Texture> ResourceManager::getDefaultWhiteTexture()
{
    return m_defaultWhiteTexture;
}

std::shared_ptr<Texture> ResourceManager::getDefaultNormalTexture()
{
    return m_defaultNormalTexture;
}

bool ResourceManager::unloadMesh(const std::string &name)
{
    std::lock_guard<std::mutex> lock(m_mtx);

    auto it = m_meshCache.find(name);
    if (it != m_meshCache.end())
    {
        m_meshCache.erase(it);
        return true;
    }
    return false;
}

bool ResourceManager::unloadTexture(const std::string &name)
{
    std::lock_guard<std::mutex> lock(m_mtx);

    auto it = m_textureCache.find(name);
    if (it != m_textureCache.end())
    {
        m_textureCache.erase(it);
        return true;
    }
    return false;
}

bool ResourceManager::unloadMaterial(const std::string &name)
{
    std::lock_guard<std::mutex> lock(m_mtx);

    auto it = m_materialCache.find(name);
    if (it != m_materialCache.end())
    {
        m_materialCache.erase(it);
        return true;
    }
    return false;
}

std::vector<std::string> ResourceManager::getMeshNames() const
{
    std::lock_guard<std::mutex> lock(m_mtx);

    std::vector<std::string> names;
    names.reserve(m_meshCache.size());

    for (const auto &pair : m_meshCache)
    {
        names.push_back(pair.first);
    }

    return names;
}

std::vector<std::string> ResourceManager::getTextureNames() const
{
    std::lock_guard<std::mutex> lock(m_mtx);

    std::vector<std::string> names;
    names.reserve(m_textureCache.size());

    for (const auto &pair : m_textureCache)
    {
        names.push_back(pair.first);
    }

    return names;
}

std::vector<std::string> ResourceManager::getMaterialNames() const
{
    std::lock_guard<std::mutex> lock(m_mtx);

    std::vector<std::string> names;
    names.reserve(m_materialCache.size());

    for (const auto &pair : m_materialCache)
    {
        names.push_back(pair.first);
    }

    return names;
}

// ==================== 描述符布局访问接口 ====================

vk::DescriptorSetLayout ResourceManager::getMaterialLayout() const
{

    if (!m_initialized)
    {
        throw std::runtime_error("ResourceManager not initialized");
    }

    return m_materialLayout;
}

bool ResourceManager::isInitialized() const
{
    std::lock_guard<std::mutex> lock(m_mtx);
    return m_initialized;
}

// ==================== 私有辅助函数 ====================

void ResourceManager::buildmateriallayout()
{
    // 创建材质描述符集布局
    std::vector<vk::DescriptorSetLayoutBinding> bindings = {
        // 绑定 0: 材质参数统一缓冲区
        {0, vk::DescriptorType::eUniformBuffer, 1,
         vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment},
        // 绑定 1-6: 纹理采样器
        {1, vk::DescriptorType::eCombinedImageSampler, 1, vk::ShaderStageFlagBits::eFragment}, // Base Color
        {2, vk::DescriptorType::eCombinedImageSampler, 1, vk::ShaderStageFlagBits::eFragment}, // Metallic
        {3, vk::DescriptorType::eCombinedImageSampler, 1, vk::ShaderStageFlagBits::eFragment}, // Roughness
        {4, vk::DescriptorType::eCombinedImageSampler, 1, vk::ShaderStageFlagBits::eFragment}, // Normal
        {5, vk::DescriptorType::eCombinedImageSampler, 1, vk::ShaderStageFlagBits::eFragment}, // Occlusion
        {6, vk::DescriptorType::eCombinedImageSampler, 1, vk::ShaderStageFlagBits::eFragment}  // Emissive
    };

    vk::DescriptorSetLayoutCreateInfo layoutInfo = {};
    layoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());
    layoutInfo.pBindings = bindings.data();

    m_materialLayout = m_device->get().createDescriptorSetLayout(layoutInfo);
}

std::shared_ptr<vkcore::Buffer> ResourceManager::createbufferfromdata(const void *data, vk::DeviceSize size,
                                                                      vk::BufferUsageFlags usage)
{
    // 创建暂存缓冲区
    vkcore::BufferDesc stagingDesc = {};
    stagingDesc.size = size;
    stagingDesc.usageFlags = vk::BufferUsageFlagBits::eTransferSrc;
    stagingDesc.memoryUsage = VMA_MEMORY_USAGE_CPU_ONLY;

    auto stagingBuffer = std::make_shared<vkcore::Buffer>("staging", *m_device, m_allocator, stagingDesc);

    // 复制数据到暂存缓冲区
    stagingBuffer->write(data, size);

    // 创建目标缓冲区
    vkcore::BufferDesc targetDesc = {};
    targetDesc.size = size;
    targetDesc.usageFlags = usage | vk::BufferUsageFlagBits::eTransferDst;
    targetDesc.memoryUsage = VMA_MEMORY_USAGE_GPU_ONLY;

    auto targetBuffer = std::make_shared<vkcore::Buffer>("target", *m_device, m_allocator, targetDesc);

    // 使用executeOnetime复制数据
    m_cmdManager->executeOnetime(m_device->getGraphicsQueue(), [&](vk::CommandBuffer cmd) {
        VkBufferCopy copyRegion = {};
        copyRegion.size = size;
        vkCmdCopyBuffer(cmd, stagingBuffer->get(), targetBuffer->get(), 1, &copyRegion);
    });

    return targetBuffer;
}

std::shared_ptr<vkcore::Image> ResourceManager::createimagefromdata(const void *data, int width, int height,
                                                                    vk::Format format)
{
    // 根据格式计算每像素字节数
    uint32_t formatSize = 4; // 默认RGBA
    switch (format)
    {
    case vk::Format::eR8Unorm:
        formatSize = 1;
        break;
    case vk::Format::eR8G8Unorm:
        formatSize = 2;
        break;
    case vk::Format::eR8G8B8Unorm:
        formatSize = 3;
        break;
    case vk::Format::eR8G8B8A8Unorm:
    case vk::Format::eR8G8B8A8Srgb:
        formatSize = 4;
        break;
    default:
        formatSize = 4; // 安全默认值
        break;
    }

    vk::DeviceSize imageSize = width * height * formatSize;

    // 创建暂存缓冲区
    vkcore::BufferDesc stagingDesc = {};
    stagingDesc.size = imageSize;
    stagingDesc.usageFlags = vk::BufferUsageFlagBits::eTransferSrc;
    stagingDesc.memoryUsage = VMA_MEMORY_USAGE_CPU_ONLY;

    auto stagingBuffer = std::make_shared<vkcore::Buffer>("staging", *m_device, m_allocator, stagingDesc);

    // 复制数据到暂存缓冲区
    stagingBuffer->write(data, imageSize);

    // 创建图像
    vkcore::ImageDesc imageDesc = {};
    imageDesc.imageType = vk::ImageType::e2D;
    imageDesc.format = format;
    imageDesc.extent = vk::Extent3D{static_cast<uint32_t>(width), static_cast<uint32_t>(height), 1};
    imageDesc.mipLevels = 1;
    imageDesc.arrayLayers = 1;
    imageDesc.samples = vk::SampleCountFlagBits::e1;
    imageDesc.tiling = vk::ImageTiling::eOptimal;
    imageDesc.usage = vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eSampled;
    imageDesc.memoryUsage = VMA_MEMORY_USAGE_GPU_ONLY;

    auto image = std::make_shared<vkcore::Image>("texture", *m_device, m_allocator, imageDesc);

    // 使用executeOnetime复制数据
    m_cmdManager->executeOnetime(m_device->getGraphicsQueue(), [&](vk::CommandBuffer cmd) {
        // 转换图像布局到传输目标
        vk::ImageMemoryBarrier barrier = {};
        barrier.oldLayout = vk::ImageLayout::eUndefined;
        barrier.newLayout = vk::ImageLayout::eTransferDstOptimal;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.image = image->get();
        barrier.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
        barrier.subresourceRange.baseMipLevel = 0;
        barrier.subresourceRange.levelCount = 1;
        barrier.subresourceRange.baseArrayLayer = 0;
        barrier.subresourceRange.layerCount = 1;
        barrier.srcAccessMask = vk::AccessFlagBits{};
        barrier.dstAccessMask = vk::AccessFlagBits::eTransferWrite;

        cmd.pipelineBarrier(vk::PipelineStageFlagBits::eTopOfPipe, vk::PipelineStageFlagBits::eTransfer,
                            vk::DependencyFlags{}, {}, {}, barrier);

        // 从缓冲区复制到图像
        VkBufferImageCopy region = {};
        region.bufferOffset = 0;
        region.bufferRowLength = 0;
        region.bufferImageHeight = 0;
        region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        region.imageSubresource.mipLevel = 0;
        region.imageSubresource.baseArrayLayer = 0;
        region.imageSubresource.layerCount = 1;
        region.imageOffset = {0, 0, 0};
        region.imageExtent = {static_cast<uint32_t>(width), static_cast<uint32_t>(height), 1};

        vkCmdCopyBufferToImage(cmd, stagingBuffer->get(), image->get(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1,
                               &region);

        // 转换布局到着色器只读
        barrier.oldLayout = vk::ImageLayout::eTransferDstOptimal;
        barrier.newLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
        barrier.srcAccessMask = vk::AccessFlagBits::eTransferWrite;
        barrier.dstAccessMask = vk::AccessFlagBits::eShaderRead;

        cmd.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eFragmentShader,
                            vk::DependencyFlags{}, {}, {}, barrier);
    });

    // 更新图像布局跟踪
    image->setCurrentLayout(vk::ImageLayout::eShaderReadOnlyOptimal);

    return image;
}

void ResourceManager::createdefaulttextures()
{
    // 创建1x1白色纹理（不需要锁，因为已经在 initialize 中获取了锁）
    unsigned char whitePixel[4] = {255, 255, 255, 255};

    // 直接创建图像而不通过 registerTexture（避免双重加锁）
    auto whiteImage = createimagefromdata(whitePixel, 1, 1, vk::Format::eR8G8B8A8Unorm);
    auto whiteSampler = getorsampler(vk::Filter::eLinear, vk::SamplerAddressMode::eRepeat);

    m_defaultWhiteTexture = std::make_shared<Texture>();
    m_defaultWhiteTexture->name = "__default_white__";
    m_defaultWhiteTexture->image = whiteImage;
    m_defaultWhiteTexture->sampler = vk::UniqueSampler(whiteSampler, m_device->get());

    // 添加到缓存
    m_textureCache["__default_white__"] = m_defaultWhiteTexture;

    // 创建1x1法线贴图 (128, 128, 255, 255) - 法线向量 (0, 0, 1)
    unsigned char normalPixel[4] = {128, 128, 255, 255};

    auto normalImage = createimagefromdata(normalPixel, 1, 1, vk::Format::eR8G8B8A8Unorm);
    auto normalSampler = getorsampler(vk::Filter::eLinear, vk::SamplerAddressMode::eRepeat);

    m_defaultNormalTexture = std::make_shared<Texture>();
    m_defaultNormalTexture->name = "__default_normal__";
    m_defaultNormalTexture->image = normalImage;
    m_defaultNormalTexture->sampler = vk::UniqueSampler(normalSampler, m_device->get());

    // 添加到缓存
    m_textureCache["__default_normal__"] = m_defaultNormalTexture;
}

void ResourceManager::createMaterialUniformBuffer(std::shared_ptr<Material> material)
{
    // 定义材质Uniform Buffer结构（匹配着色器中的结构）
    struct MaterialUniform
    {
        glm::vec4 baseColorFactor;
        glm::vec3 emissiveFactor;
        float metallicFactor;
        float roughnessFactor;
        float normalScale;
        float alphaCutoff;
        float padding; // 对齐到16字节边界
    };

    MaterialUniform uniformData = {};
    uniformData.baseColorFactor = material->baseColorFactor;
    uniformData.emissiveFactor = material->emissiveFactor;
    uniformData.metallicFactor = material->metallicFactor;
    uniformData.roughnessFactor = material->roughnessFactor;
    uniformData.normalScale = material->normalScale;
    uniformData.alphaCutoff = material->alphaCutoff;

    // 创建Uniform Buffer
    material->uniformBuffer =
        createbufferfromdata(&uniformData, sizeof(MaterialUniform), vk::BufferUsageFlagBits::eUniformBuffer);
}

vk::Sampler ResourceManager::getorsampler(vk::Filter filter, vk::SamplerAddressMode addressMode)
{
    // 创建采样器配置哈希
    uint64_t hash = std::hash<uint32_t>{}(static_cast<uint32_t>(filter)) ^
                    (std::hash<uint32_t>{}(static_cast<uint32_t>(addressMode)) << 1);

    auto it = m_samplerCache.find(hash);
    if (it != m_samplerCache.end())
    {
        return it->second;
    }

    // 创建新的采样器
    vk::SamplerCreateInfo samplerInfo = {};
    samplerInfo.magFilter = filter;
    samplerInfo.minFilter = filter;
    samplerInfo.addressModeU = addressMode;
    samplerInfo.addressModeV = addressMode;
    samplerInfo.addressModeW = addressMode;
    samplerInfo.anisotropyEnable = VK_TRUE;
    samplerInfo.maxAnisotropy = 16.0f;
    samplerInfo.borderColor = vk::BorderColor::eIntOpaqueBlack;
    samplerInfo.unnormalizedCoordinates = VK_FALSE;
    samplerInfo.compareEnable = VK_FALSE;
    samplerInfo.compareOp = vk::CompareOp::eAlways;
    samplerInfo.mipmapMode = vk::SamplerMipmapMode::eLinear;

    vk::Sampler sampler = m_device->get().createSampler(samplerInfo);
    m_samplerCache[hash] = sampler;

    return sampler;
}

void ResourceManager::updateMaterialDescriptorSet(std::shared_ptr<Material> material)
{
    // 1. 准备Uniform Buffer描述符信息
    vk::DescriptorBufferInfo bufferInfo = {};
    bufferInfo.buffer = material->uniformBuffer->get();
    bufferInfo.offset = 0;
    bufferInfo.range = VK_WHOLE_SIZE;

    // 2. 准备所有纹理的描述符信息
    std::vector<vk::DescriptorImageInfo> imageInfos(6);

    // Base Color Texture (binding 1)
    imageInfos[0].imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
    imageInfos[0].imageView = material->baseColorTexture->image->getView();
    imageInfos[0].sampler = *material->baseColorTexture->sampler;

    // Metallic Texture (binding 2)
    imageInfos[1].imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
    imageInfos[1].imageView = material->metallicTexture->image->getView();
    imageInfos[1].sampler = *material->metallicTexture->sampler;

    // Roughness Texture (binding 3)
    imageInfos[2].imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
    imageInfos[2].imageView = material->roughnessTexture->image->getView();
    imageInfos[2].sampler = *material->roughnessTexture->sampler;

    // Normal Texture (binding 4)
    imageInfos[3].imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
    imageInfos[3].imageView = material->normalTexture->image->getView();
    imageInfos[3].sampler = *material->normalTexture->sampler;

    // Occlusion Texture (binding 5)
    imageInfos[4].imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
    imageInfos[4].imageView = material->occlusionTexture->image->getView();
    imageInfos[4].sampler = *material->occlusionTexture->sampler;

    // Emissive Texture (binding 6)
    imageInfos[5].imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
    imageInfos[5].imageView = material->emissiveTexture->image->getView();
    imageInfos[5].sampler = *material->emissiveTexture->sampler;

    // 使用 DescriptorUpdater 批量更新所有绑定
    auto updater = vkcore::DescriptorUpdater::begin(*m_device, material->descriptorSet);

    // 绑定 Uniform Buffer 到 binding 0
    updater.writeBuffer(0, vk::DescriptorType::eUniformBuffer, bufferInfo);

    // 绑定所有纹理到对应的binding点
    for (int i = 0; i < 6; ++i)
    {
        updater.writeImage(i + 1, vk::DescriptorType::eCombinedImageSampler, imageInfos[i]);
    }

    // 提交更新
    updater.update();
}

std::shared_ptr<Material> ResourceManager::buildmaterial(const std::string &name, const Material &materialInfo,
                                                         const TexturePaths &textureNames,
                                                         const std::string &shaderName)
{
    auto material = std::make_shared<Material>(materialInfo);
    material->name = name;

    // 加载纹理 (如果路径不为空) - 使用内部无锁版本避免死锁
    if (!textureNames.baseColor.empty())
    {
        material->baseColorTexture = loadanduploadtexture(textureNames.baseColor, false);
    }
    else
    {
        material->baseColorTexture = m_defaultWhiteTexture;
    }

    if (!textureNames.metallic.empty())
    {
        material->metallicTexture = loadanduploadtexture(textureNames.metallic, false);
    }
    else
    {
        material->metallicTexture = m_defaultWhiteTexture;
    }

    if (!textureNames.roughness.empty())
    {
        material->roughnessTexture = loadanduploadtexture(textureNames.roughness, false);
    }
    else
    {
        material->roughnessTexture = m_defaultWhiteTexture;
    }

    if (!textureNames.normal.empty())
    {
        material->normalTexture = loadanduploadtexture(textureNames.normal, false);
    }
    else
    {
        material->normalTexture = m_defaultNormalTexture;
    }

    if (!textureNames.occlusion.empty())
    {
        material->occlusionTexture = loadanduploadtexture(textureNames.occlusion, false);
    }
    else
    {
        material->occlusionTexture = m_defaultWhiteTexture;
    }

    if (!textureNames.emissive.empty())
    {
        material->emissiveTexture = loadanduploadtexture(textureNames.emissive, false);
    }
    else
    {
        material->emissiveTexture = m_defaultWhiteTexture;
    }

    // 加载着色器
    if (!shaderName.empty())
    {
        material->vertexShader = m_shaderManager->getShaderModule(shaderName + ".vert");
        material->fragmentShader = m_shaderManager->getShaderModule(shaderName + ".frag");
    }

    // 分配描述符集
    material->descriptorSet = m_descAllocator->allocate(m_materialLayout);

    // 创建材质参数Uniform Buffer
    createMaterialUniformBuffer(material);

    // 更新描述符集 - 绑定纹理和采样器
    updateMaterialDescriptorSet(material);

    return material;
}

std::shared_ptr<Texture> ResourceManager::loadanduploadtexture(const std::filesystem::path &filepath, bool srgb)
{
    // 加载纹理数据
    TextureData textureData = TextureLoader::loadFromFile(filepath);

    if (!textureData.isValid())
    {
        throw std::runtime_error("Failed to load texture: " + filepath.string());
    }

    // 确定格式
    vk::Format format = vk::Format::eR8G8B8A8Unorm;
    if (srgb)
    {
        format = vk::Format::eR8G8B8A8Srgb;
    }

    // 创建纹理
    auto texture =
        registerTexture(filepath.string(), textureData.pixels, textureData.width, textureData.height, format);

    // 释放CPU内存
    textureData.free();

    return texture;
}

MeshData ResourceManager::mergeMeshData(const std::vector<MeshData> &meshDataList, const std::string &baseName)
{
    MeshData mergedMesh;
    mergedMesh.name = baseName;

    // 计算总的顶点和索引数量
    size_t totalVertices = 0;
    size_t totalIndices = 0;

    for (const auto &meshData : meshDataList)
    {
        totalVertices += meshData.vertices.size();
        totalIndices += meshData.indices.size();
    }

    // 预分配内存以提高性能
    mergedMesh.vertices.reserve(totalVertices);
    mergedMesh.indices.reserve(totalIndices);

    // 合并所有网格
    uint32_t vertexOffset = 0;

    for (const auto &meshData : meshDataList)
    {
        // 复制顶点数据
        mergedMesh.vertices.insert(mergedMesh.vertices.end(), meshData.vertices.begin(), meshData.vertices.end());

        // 复制索引数据（需要调整索引偏移）
        for (uint32_t index : meshData.indices)
        {
            mergedMesh.indices.push_back(index + vertexOffset);
        }

        vertexOffset += static_cast<uint32_t>(meshData.vertices.size());

        // 可选：输出合并信息
        std::cout << "Merged mesh: " << meshData.name << " (" << meshData.vertices.size() << " vertices, "
                  << meshData.indices.size() << " indices)" << std::endl;
    }

    std::cout << "Total merged: " << mergedMesh.vertices.size() << " vertices, " << mergedMesh.indices.size()
              << " indices" << std::endl;

    return mergedMesh;
}

std::shared_ptr<Mesh> ResourceManager::loadanduploadmesh(const std::filesystem::path &filepath)
{
    // 从文件加载模型数据到内存
    std::vector<MeshData> meshDataList = ModelLoader::loadModel(filepath);

    if (meshDataList.empty())
    {
        throw std::runtime_error("No meshes found in file: " + filepath.string());
    }

    // 合并所有网格为单一网格（优化方案）
    MeshData mergedMeshData = mergeMeshData(meshDataList, filepath.stem().string());

    if (!mergedMeshData.isValid())
    {
        throw std::runtime_error("Invalid mesh data loaded from file: " + filepath.string());
    }

    // 将内存数据上传到GPU，创建GPU资源（不通过 registerMesh 避免双重加锁）
    std::string name = filepath.string();

    // 检查是否已存在（不需要锁，因为调用者已经有锁了）
    auto it = m_meshCache.find(name);
    if (it != m_meshCache.end())
    {
        return it->second;
    }

    // 创建缓冲区
    auto vertexBuffer =
        createbufferfromdata(mergedMeshData.vertices.data(), mergedMeshData.vertices.size() * sizeof(Vertex),
                             vk::BufferUsageFlagBits::eVertexBuffer);

    auto indexBuffer =
        createbufferfromdata(mergedMeshData.indices.data(), mergedMeshData.indices.size() * sizeof(uint32_t),
                             vk::BufferUsageFlagBits::eIndexBuffer);

    // 创建 Mesh 对象
    auto mesh = std::make_shared<Mesh>();
    mesh->name = name;
    mesh->vertexBuffer = vertexBuffer;
    mesh->indexBuffer = indexBuffer;
    mesh->vertexCount = static_cast<uint32_t>(mergedMeshData.vertices.size());
    mesh->indexCount = static_cast<uint32_t>(mergedMeshData.indices.size());

    // 添加到缓存
    m_meshCache[name] = mesh;

    return mesh;
}

} // namespace rendercore