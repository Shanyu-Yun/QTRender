#pragma once

#include "ResourceType.hpp"
#include <algorithm>
#include <array>
#include <filesystem>
#include <fstream>
#include <nlohmann/json.hpp>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <tuple>
#include <vector>

namespace rendercore
{

// ============================================================================
// 纯内存数据结构（不包含GPU资源）
// ============================================================================

/**
 * @struct MeshData
 * @brief 从文件加载的网格原始数据（仅内存，不包含GPU缓冲区）
 */
struct MeshData
{
    std::string name;              ///< 网格名称
    std::vector<Vertex> vertices;  ///< 顶点数据
    std::vector<uint32_t> indices; ///< 索引数据

    /**
     * @brief 检查数据是否有效
     */
    bool isValid() const
    {
        return !vertices.empty();
    }

    /**
     * @brief 获取顶点数据大小（字节）
     */
    size_t getVertexDataSize() const
    {
        return vertices.size() * sizeof(Vertex);
    }

    /**
     * @brief 获取索引数据大小（字节）
     */
    size_t getIndexDataSize() const
    {
        return indices.size() * sizeof(uint32_t);
    }
};

/**
 * @struct TexturePaths
 * @brief 材质纹理路径集合
 */
struct TexturePaths
{
    std::string baseColor;
    std::string metallic;  ///< 独立金属度纹理
    std::string roughness; ///< 独立粗糙度纹理
    std::string normal;
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
 * @details 专注于文件到内存的加载，返回纯内存数据，不涉及GPU资源
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
    static ModelFormat detectFormat(const std::filesystem::path &filePath);

    /**
     * @brief 从 OBJ 文件加载模型数据到内存
     * @param filePath OBJ 文件路径
     * @param flipUVs 是否翻转 UV 坐标（默认 false）
     * @return std::vector<MeshData> 纯内存网格数据列表
     * @throws std::runtime_error 如果文件不存在或格式错误
     */
    static std::vector<MeshData> loadOBJ(const std::filesystem::path &filePath, bool flipUVs = false);

    /**
     * @brief 从 STL 文件加载模型数据到内存（二进制或 ASCII）
     * @param filePath STL 文件路径
     * @return MeshData 结构体（纯内存顶点和索引数据）
     * @throws std::runtime_error 如果文件不存在或格式错误
     */
    static MeshData loadSTL(const std::filesystem::path &filePath);

    /**
     * @brief 通用模型加载接口（自动检测格式）
     * @param filePath 模型文件路径
     * @return 纯内存网格数据列表
     * @throws std::runtime_error 如果格式不支持或加载失败
     */
    static std::vector<MeshData> loadModel(const std::filesystem::path &filePath);

  private:
    /**
     * @brief 加载二进制 STL 文件
     */
    static MeshData loadSTLBinary(std::ifstream &file);

    /**
     * @brief 加载 ASCII STL 文件
     */
    static MeshData loadSTLAscii(std::ifstream &file);
};

// ============================================================================
// 纹理加载工具
// ============================================================================

/**
 * @class TextureLoader
 * @brief 纹理文件加载工具（支持 JPG、PNG、TGA、BMP、PSD、GIF、HDR、PIC 等）
 * @details 专注于文件到内存的加载，返回纯内存像素数据，不涉及GPU资源
 *          使用 stb_image 库进行加载，需要在某个 .cpp 文件中定义 STB_IMAGE_IMPLEMENTATION
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
        PNG, ///< PNG 图像
        JPG, ///< JPEG 图像
        TGA, ///< TGA 图像
        BMP, ///< BMP 图像
        PSD, ///< Photoshop PSD
        GIF, ///< GIF 图像
        HDR, ///< HDR 图像
        PIC, ///< PIC 图像
        PNM, ///< PNM 图像
        DDS, ///< DirectDraw Surface
        KTX, ///< Khronos Texture
        ASTC ///< ASTC 压缩纹理
    };

    /**
     * @brief 从文件扩展名推断纹理格式
     */
    static TextureFormat detectFormat(const std::filesystem::path &filePath);

    /**
     * @brief 从文件加载纹理数据（自动检测格式）
     * @param filePath 纹理文件路径
     * @param desiredChannels 期望的通道数（0=保持原始, 1=灰度, 3=RGB, 4=RGBA）
     * @param flipVertically 是否垂直翻转图像（OpenGL 需要，Vulkan 通常不需要）
     * @return TextureData 结构体（调用者需要调用 free() 释放内存）
     * @throws std::runtime_error 如果文件不存在或加载失败
     */
    static TextureData loadFromFile(const std::filesystem::path &filePath, int desiredChannels = 0,
                                    bool flipVertically = false);

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
    static TextureData createSolidColor(int width, int height, const std::array<unsigned char, 4> &color);

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
                                          const std::array<unsigned char, 4> &color2);

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