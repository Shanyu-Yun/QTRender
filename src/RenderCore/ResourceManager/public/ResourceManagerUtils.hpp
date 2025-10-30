// RenderCore/ResourceManager/public/ResourceManagerUtils.hpp
#pragma once

#include "ResourceType.hpp"
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <nlohmann/json.hpp>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace rendercore
{

/**
 * @struct TexturePaths
 * @brief 材质纹理路径集合
 */
struct TexturePaths
{
    std::string baseColor;
    std::string normal;
    std::string metallicRoughness;
    std::string occlusion;
    std::string emissive;
};

/**
 * @struct MaterialData
 * @brief 从 JSON 加载的完整材质数据
 */
struct MaterialData
{
    Material material;         ///< 材质配置
    TexturePaths texturePaths; ///< 纹理路径
    std::string shaderName;    ///< 着色器名称
};

// 前向声明辅助函数
inline AlphaMode stringToAlphaMode(const std::string &str);

/**
 * @class MaterialLoader
 * @brief 材质 JSON 文件加载工具
 */
class MaterialLoader
{
  private:
    /**
     * @brief 通用 JSON 加载函数（避免代码重复）
     * @param filePath JSON 文件路径
     * @return 解析后的 JSON 对象
     * @throws std::runtime_error 如果文件不存在或 JSON 格式错误
     */
    static nlohmann::json loadJsonFile(const std::filesystem::path &filePath);

    /**
     * @brief 安全地从 JSON 数组解析 glm::vec3
     */
    static std::optional<glm::vec3> parseVec3(const nlohmann::json &arr);

    /**
     * @brief 安全地从 JSON 数组解析 glm::vec4
     */
    static std::optional<glm::vec4> parseVec4(const nlohmann::json &arr);

  public:
    /**
     * @brief 从 JSON 文件一次性加载所有材质数据（推荐使用）
     * @param filePath JSON 文件路径
     * @return MaterialData 结构体，包含材质配置、纹理路径和着色器名称
     * @throws std::runtime_error 如果文件不存在或 JSON 格式错误
     */
    static MaterialData loadMaterialData(const std::filesystem::path &filePath);

    /**
     * @brief 从 JSON 文件加载材质配置
     * @param filePath JSON 文件路径
     * @return Material 结构体（不包含已加载的纹理和着色器，仅配置数据）
     * @throws std::runtime_error 如果文件不存在或 JSON 格式错误
     */
    static Material loadFromJson(const std::filesystem::path &filePath);

    /**
     * @brief 从 JSON 对象解析材质配置
     * @param j JSON 对象
     * @return Material 结构体
     */
    static Material parseMaterial(const nlohmann::json &j);

    /**
     * @brief 从 JSON 获取纹理路径
     * @param filePath JSON 文件路径
     * @return 包含各种纹理路径的结构体
     * @throws std::runtime_error 如果文件不存在或 JSON 格式错误
     */
    static TexturePaths getTexturePaths(const std::filesystem::path &filePath);

    /**
     * @brief 从 JSON 对象解析纹理路径
     * @param j JSON 对象
     * @return TexturePaths 结构体
     */
    static TexturePaths parseTexturePaths(const nlohmann::json &j);

    /**
     * @brief 获取着色器名称
     * @param filePath JSON 文件路径
     * @return 着色器名称（例如 "PBR_Mesh"）
     * @throws std::runtime_error 如果文件不存在或 JSON 格式错误
     */
    static std::string getShaderName(const std::filesystem::path &filePath);
};

/**
 * @brief 辅助函数：将 AlphaMode 转换为字符串
 */
inline std::string alphaModeToString(AlphaMode mode)
{
    switch (mode)
    {
    case AlphaMode::Opaque:
        return "Opaque";
    case AlphaMode::Mask:
        return "Mask";
    case AlphaMode::Blend:
        return "Blend";
    default:
        return "Unknown";
    }
}

/**
 * @brief 辅助函数：将字符串转换为 AlphaMode
 */
inline AlphaMode stringToAlphaMode(const std::string &str)
{
    if (str == "Opaque")
        return AlphaMode::Opaque;
    if (str == "Mask")
        return AlphaMode::Mask;
    if (str == "Blend" || str == "Transparent")
        return AlphaMode::Blend;
    return AlphaMode::Opaque; // 默认
}

// ============================================================================
// 模型加载工具
// ============================================================================

/**
 * @class ModelLoader
 * @brief 3D 模型文件加载工具（支持 OBJ、STL 等格式）
 */
class ModelLoader
{
  public:
    /**
     * @enum ModelFormat
     * @brief 支持的模型文件格式
     */
    enum class ModelFormat
    {
        Unknown,
        OBJ, ///< Wavefront OBJ 格式
        STL, ///< STereoLithography 格式（二进制或ASCII）
        PLY, ///< Polygon File Format
        FBX, ///< Autodesk FBX 格式（需要 Assimp）
        GLTF ///< glTF 2.0 格式（需要 tinygltf）
    };

    /**
     * @brief 从文件扩展名推断模型格式
     */
    static ModelFormat detectFormat(const std::filesystem::path &filePath)
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

    /**
     * @brief 从 OBJ 文件加载模型
     * @param filePath OBJ 文件路径
     * @param flipUVs 是否翻转 UV 坐标（默认 false）
     * @return Mesh 结构体（仅包含顶点和索引数据，未创建 GPU 缓冲区）
     * @throws std::runtime_error 如果文件不存在或格式错误
     */
    static std::vector<Mesh> loadOBJ(const std::filesystem::path &filePath, bool flipUVs = false)
    {
        if (!std::filesystem::exists(filePath))
        {
            throw std::runtime_error("OBJ file not found: " + filePath.string());
        }

        std::vector<Mesh> meshes;
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
            std::istringstream iss(line);
            std::string prefix;
            iss >> prefix;

            if (prefix == "o" || prefix == "g")
            {
                // 新的对象/组，保存之前的网格
                if (hasData && !vertices.empty())
                {
                    Mesh mesh;
                    mesh.name = currentMeshName;
                    mesh.vertexCount = static_cast<uint32_t>(vertices.size());
                    mesh.indexCount = static_cast<uint32_t>(indices.size());
                    // 注意：这里不创建 GPU 缓冲区，由调用者负责
                    meshes.push_back(mesh);

                    vertices.clear();
                    indices.clear();
                }

                iss >> currentMeshName;
                hasData = true;
            }
            else if (prefix == "v")
            {
                // 顶点位置
                glm::vec3 pos;
                iss >> pos.x >> pos.y >> pos.z;
                positions.push_back(pos);
            }
            else if (prefix == "vn")
            {
                // 法线
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
            Mesh mesh;
            mesh.name = currentMeshName;
            mesh.vertexCount = static_cast<uint32_t>(vertices.size());
            mesh.indexCount = static_cast<uint32_t>(indices.size());
            meshes.push_back(mesh);
        }

        if (meshes.empty())
        {
            throw std::runtime_error("No geometry found in OBJ file: " + filePath.string());
        }

        return meshes;
    }

    /**
     * @brief 从 STL 文件加载模型（二进制或 ASCII）
     * @param filePath STL 文件路径
     * @return Mesh 结构体（仅包含顶点和索引数据）
     * @throws std::runtime_error 如果文件不存在或格式错误
     */
    static Mesh loadSTL(const std::filesystem::path &filePath)
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

        Mesh mesh;
        mesh.name = filePath.stem().string();

        if (isBinary)
        {
            mesh = loadSTLBinary(file);
        }
        else
        {
            mesh = loadSTLAscii(file);
        }

        file.close();
        return mesh;
    }

    /**
     * @brief 通用模型加载接口（自动检测格式）
     * @param filePath 模型文件路径
     * @return 网格列表
     * @throws std::runtime_error 如果格式不支持或加载失败
     */
    static std::vector<Mesh> loadModel(const std::filesystem::path &filePath)
    {
        ModelFormat format = detectFormat(filePath);

        switch (format)
        {
        case ModelFormat::OBJ:
            return loadOBJ(filePath);

        case ModelFormat::STL: {
            std::vector<Mesh> meshes;
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

  private:
    /**
     * @brief 加载二进制 STL 文件
     */
    static Mesh loadSTLBinary(std::ifstream &file)
    {
        Mesh mesh;
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

        mesh.vertexCount = static_cast<uint32_t>(vertices.size());
        mesh.indexCount = static_cast<uint32_t>(indices.size());

        return mesh;
    }

    /**
     * @brief 加载 ASCII STL 文件
     */
    static Mesh loadSTLAscii(std::ifstream &file)
    {
        Mesh mesh;
        std::vector<Vertex> vertices;
        std::vector<uint32_t> indices;

        std::string line;
        glm::vec3 normal;

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

        mesh.vertexCount = static_cast<uint32_t>(vertices.size());
        mesh.indexCount = static_cast<uint32_t>(indices.size());

        return mesh;
    }
};

// ============================================================================
// 纹理加载工具
// ============================================================================

/**
 * @struct TextureData
 * @brief 从文件加载的纹理原始数据
 */
struct TextureData
{
    unsigned char *pixels{nullptr}; ///< 像素数据指针（调用者负责释放）
    int width{0};                   ///< 图像宽度（像素）
    int height{0};                  ///< 图像高度（像素）
    int channels{0};                ///< 通道数（1=灰度, 2=灰度+alpha, 3=RGB, 4=RGBA）
    size_t dataSize{0};             ///< 数据大小（字节）

    /**
     * @brief 释放纹理数据（使用 stbi_image_free）
     */
    void free();

    /**
     * @brief 检查数据是否有效
     */
    bool isValid() const
    {
        return pixels != nullptr && width > 0 && height > 0;
    }
};

/**
 * @class TextureLoader
 * @brief 纹理文件加载工具（支持 JPG、PNG、TGA、BMP、PSD、GIF、HDR、PIC 等）
 * @details 使用 stb_image 库进行加载，需要在某个 .cpp 文件中定义 STB_IMAGE_IMPLEMENTATION
 */
class TextureLoader
{
  public:
    /**
     * @enum TextureFormat
     * @brief 支持的纹理文件格式
     */
    enum class TextureFormat
    {
        Unknown,
        PNG, ///< PNG 格式（推荐用于透明纹理）
        JPG, ///< JPEG 格式（推荐用于照片）
        TGA, ///< Targa 格式
        BMP, ///< Windows Bitmap 格式
        PSD, ///< Photoshop 格式
        GIF, ///< GIF 格式（仅第一帧）
        HDR, ///< Radiance HDR 格式（浮点数据）
        PIC, ///< Softimage PIC 格式
        PNM, ///< Portable aNyMap 格式
        DDS, ///< DirectDraw Surface 格式（需要额外支持）
        KTX, ///< Khronos Texture 格式（需要额外支持）
        ASTC ///< ARM Adaptive Scalable Texture Compression（需要额外支持）
    };

    /**
     * @brief 从文件扩展名推断纹理格式
     */
    static TextureFormat detectFormat(const std::filesystem::path &filePath)
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
        if (ext == ".pnm" || ext == ".pgm" || ext == ".ppm")
            return TextureFormat::PNM;
        if (ext == ".dds")
            return TextureFormat::DDS;
        if (ext == ".ktx" || ext == ".ktx2")
            return TextureFormat::KTX;
        if (ext == ".astc")
            return TextureFormat::ASTC;

        return TextureFormat::Unknown;
    }

    /**
     * @brief 从文件加载纹理数据（自动检测格式）
     * @param filePath 纹理文件路径
     * @param desiredChannels 期望的通道数（0=保持原始, 1=灰度, 3=RGB, 4=RGBA）
     * @param flipVertically 是否垂直翻转图像（OpenGL 需要，Vulkan 通常不需要）
     * @return TextureData 结构体（调用者需要调用 free() 释放内存）
     * @throws std::runtime_error 如果文件不存在或加载失败
     */
    static TextureData loadFromFile(const std::filesystem::path &filePath, int desiredChannels = 0,
                                    bool flipVertically = false)
    {
        if (!std::filesystem::exists(filePath))
        {
            throw std::runtime_error("Texture file not found: " + filePath.string());
        }

        TextureFormat format = detectFormat(filePath);

        // 处理特殊格式
        if (format == TextureFormat::HDR)
        {
            return loadHDR(filePath, desiredChannels, flipVertically);
        }
        else if (format == TextureFormat::DDS || format == TextureFormat::KTX || format == TextureFormat::ASTC)
        {
            throw std::runtime_error("Compressed texture formats (DDS/KTX/ASTC) require specialized loaders");
        }

        // 使用 stb_image 加载标准格式
        return loadStandard(filePath, desiredChannels, flipVertically);
    }

    /**
     * @brief 从内存加载纹理数据
     * @param data 内存数据指针
     * @param dataSize 数据大小（字节）
     * @param desiredChannels 期望的通道数
     * @param flipVertically 是否垂直翻转
     * @return TextureData 结构体
     */
    static TextureData loadFromMemory(const unsigned char *data, size_t dataSize, int desiredChannels = 0,
                                      bool flipVertically = false);

    /**
     * @brief 创建纯色纹理
     * @param width 宽度
     * @param height 高度
     * @param color 颜色（RGBA，每个分量 0-255）
     * @return TextureData 结构体
     */
    static TextureData createSolidColor(int width, int height, const std::array<unsigned char, 4> &color)
    {
        TextureData data;
        data.width = width;
        data.height = height;
        data.channels = 4;
        data.dataSize = width * height * 4;
        data.pixels = new unsigned char[data.dataSize];

        for (int i = 0; i < width * height; ++i)
        {
            data.pixels[i * 4 + 0] = color[0]; // R
            data.pixels[i * 4 + 1] = color[1]; // G
            data.pixels[i * 4 + 2] = color[2]; // B
            data.pixels[i * 4 + 3] = color[3]; // A
        }

        return data;
    }

    /**
     * @brief 创建棋盘格纹理（用于调试）
     * @param width 宽度
     * @param height 高度
     * @param squareSize 方格大小（像素）
     * @param color1 第一种颜色
     * @param color2 第二种颜色
     * @return TextureData 结构体
     */
    static TextureData createCheckerboard(int width, int height, int squareSize,
                                          const std::array<unsigned char, 4> &color1,
                                          const std::array<unsigned char, 4> &color2)
    {
        TextureData data;
        data.width = width;
        data.height = height;
        data.channels = 4;
        data.dataSize = width * height * 4;
        data.pixels = new unsigned char[data.dataSize];

        for (int y = 0; y < height; ++y)
        {
            for (int x = 0; x < width; ++x)
            {
                bool isColor1 = ((x / squareSize) + (y / squareSize)) % 2 == 0;
                const auto &color = isColor1 ? color1 : color2;

                int idx = (y * width + x) * 4;
                data.pixels[idx + 0] = color[0];
                data.pixels[idx + 1] = color[1];
                data.pixels[idx + 2] = color[2];
                data.pixels[idx + 3] = color[3];
            }
        }

        return data;
    }

    /**
     * @brief 获取纹理信息（不加载像素数据）
     * @param filePath 纹理文件路径
     * @return 包含宽度、高度、通道数的 tuple
     */
    static std::tuple<int, int, int> getTextureInfo(const std::filesystem::path &filePath);

  private:
    /**
     * @brief 使用 stb_image 加载标准格式
     */
    static TextureData loadStandard(const std::filesystem::path &filePath, int desiredChannels, bool flipVertically);

    /**
     * @brief 使用 stb_image 加载 HDR 格式（浮点数据）
     */
    static TextureData loadHDR(const std::filesystem::path &filePath, int desiredChannels, bool flipVertically);
};

} // namespace rendercore
