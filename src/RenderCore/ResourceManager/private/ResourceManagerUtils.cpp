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

    // 解析 alpha 模式（优先从 "alpha" 对象读取）
    if (j.contains("alpha"))
    {
        const auto &alpha = j["alpha"];

        // 读取 mode 字段
        if (alpha.contains("mode"))
        {
            material.alphaMode = stringToAlphaMode(alpha["mode"].get<std::string>());
        }

        material.alphaCutoff = alpha.value("cutoff", 0.5f);
        material.doubleSided = alpha.value("doubleSided", false);
    }

    // 向后兼容：如果没有 alpha 对象，从顶层 domain 读取
    if (j.contains("domain"))
    {
        material.alphaMode = stringToAlphaMode(j["domain"].get<std::string>());
    }

    // 解析 optical 配置（扩展属性）
    if (j.contains("optical"))
    {
        const auto &optical = j["optical"];
        material.refractionIndex = optical.value("refractionIndex", 1.0f);
    }

    // 解析 normalScale（从 factors 中）
    if (j.contains("factors") && j["factors"].contains("normalScale"))
    {
        material.normalScale = j["factors"]["normalScale"].get<float>();
    }

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

        // 支持独立的 metallic 和 roughness 纹理
        paths.baseColor = textures.value("baseColor", "");
        paths.metallic = textures.value("metallic", "");
        paths.roughness = textures.value("roughness", "");
        paths.normal = textures.value("normal", "");
        paths.occlusion = textures.value("occlusion", "");
        paths.emissive = textures.value("emissive", "");

        // 向后兼容：如果提供了合并的 metallicRoughness 纹理（glTF 2.0 标准）
        if (textures.contains("metallicRoughness"))
        {
            std::string combinedPath = textures["metallicRoughness"];
            if (paths.metallic.empty())
                paths.metallic = combinedPath;
            if (paths.roughness.empty())
                paths.roughness = combinedPath;
        }
    }

    return paths;
}

std::string MaterialLoader::getShaderName(const std::filesystem::path &filePath)
{
    return loadJsonFile(filePath).value("shader", "");
}

// ============================================================================
// ModelLoader 实现
// ============================================================================

ModelLoader::ModelFormat ModelLoader::detectFormat(const std::filesystem::path &filePath)
{
    std::string ext = filePath.extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

    if (ext == ".obj")
        return ModelFormat::OBJ;
    if (ext == ".stl")
        return ModelFormat::STL;
    if (ext == ".ply")
        return ModelFormat::PLY;
    if (ext == ".fbx")
        return ModelFormat::FBX;
    if (ext == ".gltf" || ext == ".glb")
        return ModelFormat::GLTF;

    return ModelFormat::Unknown;
}

std::vector<MeshData> ModelLoader::loadOBJ(const std::filesystem::path &filePath, bool flipUVs)
{
    if (!std::filesystem::exists(filePath))
    {
        throw std::runtime_error("OBJ file not found: " + filePath.string());
    }

    std::vector<MeshData> meshes;
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;

    // 临时存储
    std::vector<glm::vec3> positions;
    std::vector<glm::vec3> normals;
    std::vector<glm::vec2> texCoords;

    std::ifstream file(filePath);
    if (!file.is_open())
    {
        throw std::runtime_error("Failed to open OBJ file: " + filePath.string());
    }

    std::string line;
    std::string currentMeshName = "default";
    bool hasData = false;

    while (std::getline(file, line))
    {
        if (line.empty() || line[0] == '#')
            continue;

        std::istringstream iss(line);
        std::string prefix;
        iss >> prefix;

        if (prefix == "v")
        {
            // 顶点位置
            glm::vec3 pos;
            iss >> pos.x >> pos.y >> pos.z;
            positions.push_back(pos);
            hasData = true;
        }
        else if (prefix == "vn")
        {
            // 顶点法线
            glm::vec3 normal;
            iss >> normal.x >> normal.y >> normal.z;
            normals.push_back(normal);
        }
        else if (prefix == "vt")
        {
            // 纹理坐标
            glm::vec2 uv;
            iss >> uv.x >> uv.y;
            if (flipUVs)
                uv.y = 1.0f - uv.y;
            texCoords.push_back(uv);
        }
        else if (prefix == "g" || prefix == "o")
        {
            // 组或对象名（开始新的网格）
            if (hasData && !vertices.empty())
            {
                MeshData meshData;
                meshData.name = currentMeshName;
                meshData.vertices = std::move(vertices);
                meshData.indices = std::move(indices);
                meshes.push_back(std::move(meshData));

                vertices.clear();
                indices.clear();
            }
            iss >> currentMeshName;
            hasData = false;
        }
        else if (prefix == "f")
        {
            // 面（三角形）
            std::string vertexStr;
            std::vector<uint32_t> faceIndices;

            while (iss >> vertexStr)
            {
                // 解析格式：v/vt/vn 或 v//vn 或 v/vt 或 v
                int posIdx = 0, uvIdx = 0, normalIdx = 0;
                std::replace(vertexStr.begin(), vertexStr.end(), '/', ' ');
                std::istringstream viss(vertexStr);

                viss >> posIdx;
                if (viss.peek() == ' ')
                {
                    viss.ignore();
                    if (viss.peek() != ' ')
                        viss >> uvIdx;
                    if (viss.peek() == ' ')
                    {
                        viss.ignore();
                        viss >> normalIdx;
                    }
                }

                // OBJ 索引从 1 开始，转换为从 0 开始
                Vertex vertex;
                vertex.position = positions[posIdx - 1];
                vertex.normal = normalIdx > 0 ? normals[normalIdx - 1] : glm::vec3(0, 1, 0);
                vertex.texCoord = uvIdx > 0 ? texCoords[uvIdx - 1] : glm::vec2(0, 0);
                vertex.color = glm::vec4(1.0f); // 默认白色

                uint32_t index = static_cast<uint32_t>(vertices.size());
                vertices.push_back(vertex);
                faceIndices.push_back(index);
            }

            // 三角形化（如果是四边形或多边形）
            for (size_t i = 2; i < faceIndices.size(); ++i)
            {
                indices.push_back(faceIndices[0]);
                indices.push_back(faceIndices[i - 1]);
                indices.push_back(faceIndices[i]);
            }
        }
    }

    // 保存最后一个网格
    if (!vertices.empty())
    {
        MeshData meshData;
        meshData.name = currentMeshName;
        meshData.vertices = std::move(vertices);
        meshData.indices = std::move(indices);
        meshes.push_back(std::move(meshData));
    }

    if (meshes.empty())
    {
        throw std::runtime_error("No geometry found in OBJ file: " + filePath.string());
    }

    return meshes;
}

MeshData ModelLoader::loadSTL(const std::filesystem::path &filePath)
{
    if (!std::filesystem::exists(filePath))
    {
        throw std::runtime_error("STL file not found: " + filePath.string());
    }

    std::ifstream file(filePath, std::ios::binary);
    if (!file.is_open())
    {
        throw std::runtime_error("Failed to open STL file: " + filePath.string());
    }

    // 检测是二进制还是 ASCII
    char header[5];
    file.read(header, 5);
    file.seekg(0);

    bool isBinary = (std::string(header, 5) != "solid");

    MeshData meshData;
    meshData.name = filePath.stem().string();

    if (isBinary)
    {
        meshData = loadSTLBinary(file);
    }
    else
    {
        meshData = loadSTLAscii(file);
    }

    file.close();
    return meshData;
}

MeshData ModelLoader::loadSTLBinary(std::ifstream &file)
{
    MeshData meshData;
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;

    // 跳过 80 字节的头部
    file.seekg(80);

    // 读取三角形数量
    uint32_t triangleCount;
    file.read(reinterpret_cast<char *>(&triangleCount), sizeof(uint32_t));

    vertices.reserve(triangleCount * 3);
    indices.reserve(triangleCount * 3);

    for (uint32_t i = 0; i < triangleCount; ++i)
    {
        // 法线（12 字节）
        glm::vec3 normal;
        file.read(reinterpret_cast<char *>(&normal), sizeof(glm::vec3));

        // 三个顶点（每个 12 字节）
        for (int j = 0; j < 3; ++j)
        {
            Vertex vertex;
            file.read(reinterpret_cast<char *>(&vertex.position), sizeof(glm::vec3));
            vertex.normal = normal;
            vertex.texCoord = glm::vec2(0.0f);
            vertex.color = glm::vec4(1.0f);

            vertices.push_back(vertex);
            indices.push_back(static_cast<uint32_t>(vertices.size() - 1));
        }

        // 属性字节计数（2 字节，通常忽略）
        uint16_t attributeByteCount;
        file.read(reinterpret_cast<char *>(&attributeByteCount), sizeof(uint16_t));
    }

    meshData.vertices = std::move(vertices);
    meshData.indices = std::move(indices);

    return meshData;
}

MeshData ModelLoader::loadSTLAscii(std::ifstream &file)
{
    MeshData meshData;
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;

    std::string line;
    glm::vec3 normal{0.0f, 0.0f, 1.0f}; // 初始化为默认法线向量

    while (std::getline(file, line))
    {
        std::istringstream iss(line);
        std::string keyword;
        iss >> keyword;

        if (keyword == "facet")
        {
            // facet normal nx ny nz
            std::string normalStr;
            iss >> normalStr >> normal.x >> normal.y >> normal.z;
        }
        else if (keyword == "vertex")
        {
            // vertex x y z
            Vertex vertex;
            iss >> vertex.position.x >> vertex.position.y >> vertex.position.z;
            vertex.normal = normal;
            vertex.texCoord = glm::vec2(0.0f);
            vertex.color = glm::vec4(1.0f);

            vertices.push_back(vertex);
            indices.push_back(static_cast<uint32_t>(vertices.size() - 1));
        }
    }

    meshData.vertices = std::move(vertices);
    meshData.indices = std::move(indices);

    return meshData;
}

std::vector<MeshData> ModelLoader::loadModel(const std::filesystem::path &filePath)
{
    ModelFormat format = detectFormat(filePath);

    switch (format)
    {
    case ModelFormat::OBJ:
        return loadOBJ(filePath);

    case ModelFormat::STL: {
        std::vector<MeshData> meshes;
        meshes.push_back(loadSTL(filePath));
        return meshes;
    }

    case ModelFormat::FBX:
    case ModelFormat::GLTF:
    case ModelFormat::PLY:
        throw std::runtime_error("Format not yet implemented. Consider using Assimp library.");

    default:
        throw std::runtime_error("Unsupported or unknown model format: " + filePath.string());
    }
}

// ============================================================================
// TextureLoader 实现
// ============================================================================

TextureLoader::TextureFormat TextureLoader::detectFormat(const std::filesystem::path &filePath)
{
    std::string ext = filePath.extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

    if (ext == ".png")
        return TextureFormat::PNG;
    if (ext == ".jpg" || ext == ".jpeg")
        return TextureFormat::JPG;
    if (ext == ".tga")
        return TextureFormat::TGA;
    if (ext == ".bmp")
        return TextureFormat::BMP;
    if (ext == ".psd")
        return TextureFormat::PSD;
    if (ext == ".gif")
        return TextureFormat::GIF;
    if (ext == ".hdr")
        return TextureFormat::HDR;
    if (ext == ".pic")
        return TextureFormat::PIC;
    if (ext == ".pnm" || ext == ".pbm" || ext == ".pgm" || ext == ".ppm")
        return TextureFormat::PNM;
    if (ext == ".dds")
        return TextureFormat::DDS;
    if (ext == ".ktx")
        return TextureFormat::KTX;
    if (ext == ".astc")
        return TextureFormat::ASTC;

    return TextureFormat::Unknown;
}

TextureData TextureLoader::loadFromFile(const std::filesystem::path &filePath, int desiredChannels, bool flipVertically)
{
    if (!std::filesystem::exists(filePath))
    {
        throw std::runtime_error("Texture file not found: " + filePath.string());
    }

    TextureFormat format = detectFormat(filePath);

    switch (format)
    {
    case TextureFormat::HDR:
        return loadHDR(filePath, desiredChannels, flipVertically);

    case TextureFormat::PNG:
    case TextureFormat::JPG:
    case TextureFormat::TGA:
    case TextureFormat::BMP:
    case TextureFormat::PSD:
    case TextureFormat::GIF:
    case TextureFormat::PIC:
    case TextureFormat::PNM:
        return loadStandard(filePath, desiredChannels, flipVertically);

    case TextureFormat::DDS:
    case TextureFormat::KTX:
    case TextureFormat::ASTC:
        throw std::runtime_error("Compressed texture formats not yet implemented: " + filePath.string());

    default:
        throw std::runtime_error("Unsupported texture format: " + filePath.string());
    }
}

TextureData TextureLoader::createSolidColor(int width, int height, const std::array<unsigned char, 4> &color)
{
    TextureData data;
    data.width = width;
    data.height = height;
    data.channels = 4;
    data.dataSize = width * height * 4;

    data.pixels = static_cast<unsigned char *>(malloc(data.dataSize));
    if (!data.pixels)
    {
        throw std::runtime_error("Failed to allocate memory for solid color texture");
    }

    for (int i = 0; i < width * height; ++i)
    {
        data.pixels[i * 4 + 0] = color[0]; // R
        data.pixels[i * 4 + 1] = color[1]; // G
        data.pixels[i * 4 + 2] = color[2]; // B
        data.pixels[i * 4 + 3] = color[3]; // A
    }

    return data;
}

TextureData TextureLoader::createCheckerboard(int width, int height, int squareSize,
                                              const std::array<unsigned char, 4> &color1,
                                              const std::array<unsigned char, 4> &color2)
{
    TextureData data;
    data.width = width;
    data.height = height;
    data.channels = 4;
    data.dataSize = width * height * 4;

    data.pixels = static_cast<unsigned char *>(malloc(data.dataSize));
    if (!data.pixels)
    {
        throw std::runtime_error("Failed to allocate memory for checkerboard texture");
    }

    for (int y = 0; y < height; ++y)
    {
        for (int x = 0; x < width; ++x)
        {
            bool isFirstColor = ((x / squareSize) + (y / squareSize)) % 2 == 0;
            const auto &color = isFirstColor ? color1 : color2;

            int index = (y * width + x) * 4;
            data.pixels[index + 0] = color[0]; // R
            data.pixels[index + 1] = color[1]; // G
            data.pixels[index + 2] = color[2]; // B
            data.pixels[index + 3] = color[3]; // A
        }
    }

    return data;
}

TextureData TextureLoader::loadStandard(const std::filesystem::path &filePath, int desiredChannels, bool flipVertically)
{
    // 设置垂直翻转
    stbi_set_flip_vertically_on_load(static_cast<int>(flipVertically));

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
