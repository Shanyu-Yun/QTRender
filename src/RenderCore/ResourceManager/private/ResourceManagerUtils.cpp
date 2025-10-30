// RenderCore/ResourceManager/private/ResourceManagerUtils.cpp
#include "ResourceManagerUtils.hpp"

// ============================================================================
// stb_image 库集成
// ============================================================================

// 定义 STB_IMAGE_IMPLEMENTATION 以包含实现代码
#define STB_IMAGE_IMPLEMENTATION
#define STBI_FAILURE_USERMSG // 提供更详细的错误信息

// 可选：禁用某些不需要的格式以减少编译时间
// #define STBI_NO_PSD
// #define STBI_NO_TGA
// #define STBI_NO_GIF
// #define STBI_NO_HDR
// #define STBI_NO_PIC
// #define STBI_NO_PNM

// 包含 stb_image 头文件
// 注意：需要将 stb_image.h 放在包含路径中
#pragma warning(push)
#pragma warning(disable : 4996) // 禁用 MSVC 不安全函数警告
#include <stb_image.h>
#pragma warning(pop)

namespace rendercore
{

// ============================================================================
// MaterialLoader 实现
// ============================================================================

nlohmann::json MaterialLoader::loadJsonFile(const std::filesystem::path &filePath)
{
    // 检查文件是否存在
    if (!std::filesystem::exists(filePath))
    {
        throw std::runtime_error("Material JSON file not found: " + filePath.string());
    }

    // 读取 JSON 文件
    std::ifstream file(filePath);
    if (!file.is_open())
    {
        throw std::runtime_error("Failed to open material JSON file: " + filePath.string());
    }

    nlohmann::json j;
    try
    {
        file >> j;
    }
    catch (const nlohmann::json::exception &e)
    {
        throw std::runtime_error("JSON parse error in " + filePath.string() + ": " + e.what());
    }

    return j;
}

std::optional<glm::vec3> MaterialLoader::parseVec3(const nlohmann::json &arr)
{
    if (!arr.is_array() || arr.size() < 3)
        return std::nullopt;

    try
    {
        return glm::vec3(arr[0].get<float>(), arr[1].get<float>(), arr[2].get<float>());
    }
    catch (...)
    {
        return std::nullopt;
    }
}

std::optional<glm::vec4> MaterialLoader::parseVec4(const nlohmann::json &arr)
{
    if (!arr.is_array() || arr.size() < 4)
        return std::nullopt;

    try
    {
        return glm::vec4(arr[0].get<float>(), arr[1].get<float>(), arr[2].get<float>(), arr[3].get<float>());
    }
    catch (...)
    {
        return std::nullopt;
    }
}

MaterialData MaterialLoader::loadMaterialData(const std::filesystem::path &filePath)
{
    nlohmann::json j = loadJsonFile(filePath);

    MaterialData data;
    data.material = parseMaterial(j);
    data.texturePaths = parseTexturePaths(j);
    data.shaderName = j.value("shader", "");

    return data;
}

Material MaterialLoader::loadFromJson(const std::filesystem::path &filePath)
{
    return parseMaterial(loadJsonFile(filePath));
}

Material MaterialLoader::parseMaterial(const nlohmann::json &j)
{
    Material material;

    // 基本信息
    material.name = j.value("name", "Unnamed Material");

    // 解析 factors
    if (j.contains("factors"))
    {
        const auto &factors = j["factors"];

        // Base Color Factor（使用安全解析）
        if (factors.contains("baseColor"))
        {
            if (auto bc = parseVec4(factors["baseColor"]))
            {
                material.baseColorFactor = *bc;
            }
        }

        // Metallic Factor
        material.metallicFactor = factors.value("metallic", 1.0f);

        // Roughness Factor
        material.roughnessFactor = factors.value("roughness", 1.0f);

        // Emissive Factor（使用安全解析）
        if (factors.contains("emissive"))
        {
            if (auto em = parseVec3(factors["emissive"]))
            {
                material.emissiveFactor = *em;
            }
        }
    }

    // 解析 alpha 模式
    if (j.contains("alpha"))
    {
        const auto &alpha = j["alpha"];
        material.alphaCutoff = alpha.value("cutoff", 0.5f);

        bool isMask = alpha.value("mask", false);
        material.alphaMode = isMask ? AlphaMode::Mask : AlphaMode::Opaque;
    }

    // 解析 domain（渲染域）- 优先级高于 alpha.mask
    if (j.contains("domain"))
    {
        material.alphaMode = stringToAlphaMode(j["domain"].get<std::string>());
    }

    // 解析双面渲染（可选）
    material.doubleSided = j.value("doubleSided", false);

    return material;
}

TexturePaths MaterialLoader::getTexturePaths(const std::filesystem::path &filePath)
{
    return parseTexturePaths(loadJsonFile(filePath));
}

TexturePaths MaterialLoader::parseTexturePaths(const nlohmann::json &j)
{
    TexturePaths paths;

    if (j.contains("textures"))
    {
        const auto &textures = j["textures"];
        paths.baseColor = textures.value("baseColor", "");
        paths.normal = textures.value("normal", "");
        paths.metallicRoughness = textures.value("metallicRoughness", "");
        paths.occlusion = textures.value("occlusion", "");
        paths.emissive = textures.value("emissive", "");
    }

    return paths;
}

std::string MaterialLoader::getShaderName(const std::filesystem::path &filePath)
{
    return loadJsonFile(filePath).value("shader", "");
}

// ============================================================================
// TextureLoader 实现
// ============================================================================

TextureData TextureLoader::loadStandard(const std::filesystem::path &filePath, int desiredChannels, bool flipVertically)
{
    // 设置垂直翻转
    stbi_set_flip_vertically_on_load(flipVertically ? 1 : 0);

    TextureData data;

    // 加载图像
    data.pixels = stbi_load(filePath.string().c_str(), &data.width, &data.height, &data.channels, desiredChannels);

    if (!data.pixels)
    {
        std::string errorMsg = "Failed to load texture: " + filePath.string();
        const char *stbiError = stbi_failure_reason();
        if (stbiError)
        {
            errorMsg += " (Reason: " + std::string(stbiError) + ")";
        }
        throw std::runtime_error(errorMsg);
    }

    // 如果指定了期望通道数，更新实际通道数
    if (desiredChannels > 0)
    {
        data.channels = desiredChannels;
    }

    data.dataSize = data.width * data.height * data.channels;

    return data;
}

TextureData TextureLoader::loadHDR(const std::filesystem::path &filePath, int desiredChannels, bool flipVertically)
{
    // 设置垂直翻转
    stbi_set_flip_vertically_on_load(flipVertically ? 1 : 0);

    TextureData data;

    // 加载 HDR 图像（返回 float* 而不是 unsigned char*）
    float *hdrPixels =
        stbi_loadf(filePath.string().c_str(), &data.width, &data.height, &data.channels, desiredChannels);

    if (!hdrPixels)
    {
        std::string errorMsg = "Failed to load HDR texture: " + filePath.string();
        const char *stbiError = stbi_failure_reason();
        if (stbiError)
        {
            errorMsg += " (Reason: " + std::string(stbiError) + ")";
        }
        throw std::runtime_error(errorMsg);
    }

    // 如果指定了期望通道数，更新实际通道数
    if (desiredChannels > 0)
    {
        data.channels = desiredChannels;
    }

    // 将 float* 转换为 unsigned char*（需要调用者知道这是浮点数据）
    // 注意：这里只是类型转换，实际数据仍然是 float
    data.pixels = reinterpret_cast<unsigned char *>(hdrPixels);
    data.dataSize = data.width * data.height * data.channels * sizeof(float);

    return data;
}

TextureData TextureLoader::loadFromMemory(const unsigned char *data, size_t dataSize, int desiredChannels,
                                          bool flipVertically)
{
    // 设置垂直翻转
    stbi_set_flip_vertically_on_load(flipVertically ? 1 : 0);

    TextureData result;

    // 从内存加载图像
    result.pixels = stbi_load_from_memory(data, static_cast<int>(dataSize), &result.width, &result.height,
                                          &result.channels, desiredChannels);

    if (!result.pixels)
    {
        std::string errorMsg = "Failed to load texture from memory";
        const char *stbiError = stbi_failure_reason();
        if (stbiError)
        {
            errorMsg += " (Reason: " + std::string(stbiError) + ")";
        }
        throw std::runtime_error(errorMsg);
    }

    // 如果指定了期望通道数，更新实际通道数
    if (desiredChannels > 0)
    {
        result.channels = desiredChannels;
    }

    result.dataSize = result.width * result.height * result.channels;

    return result;
}

std::tuple<int, int, int> TextureLoader::getTextureInfo(const std::filesystem::path &filePath)
{
    if (!std::filesystem::exists(filePath))
    {
        throw std::runtime_error("Texture file not found: " + filePath.string());
    }

    int width, height, channels;

    // 只获取图像信息，不加载像素数据
    if (!stbi_info(filePath.string().c_str(), &width, &height, &channels))
    {
        std::string errorMsg = "Failed to get texture info: " + filePath.string();
        const char *stbiError = stbi_failure_reason();
        if (stbiError)
        {
            errorMsg += " (Reason: " + std::string(stbiError) + ")";
        }
        throw std::runtime_error(errorMsg);
    }

    return {width, height, channels};
}

// ============================================================================
// TextureData 内存管理
// ============================================================================

void TextureData::free()
{
    if (pixels)
    {
        // 使用 stbi_image_free 释放 stb_image 分配的内存
        stbi_image_free(pixels);
        pixels = nullptr;
    }
}

} // namespace rendercore
