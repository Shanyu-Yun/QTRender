# ModelLoader 使用示例

## 功能概述

`ModelLoader` 类提供了加载 3D 模型文件的功能，目前支持：
- ✅ **OBJ** (Wavefront) - 完全支持
- ✅ **STL** (STereoLithography) - 二进制和 ASCII 格式
- ⏳ **PLY** (Polygon File Format) - 待实现
- ⏳ **FBX** (Autodesk) - 需要 Assimp
- ⏳ **glTF 2.0** - 需要 tinygltf

## 基本使用

### 1. 自动检测格式加载

```cpp
#include "RenderCore/ResourceManager/public/ResourceManagerUtils.hpp"

using namespace rendercore;

// 自动检测并加载模型（推荐）
try {
    std::vector<Mesh> meshes = ModelLoader::loadModel("assets/models/car.obj");
    
    for (const auto& mesh : meshes) {
        std::cout << "Mesh: " << mesh.name << std::endl;
        std::cout << "  Vertices: " << mesh.vertexCount << std::endl;
        std::cout << "  Indices: " << mesh.indexCount << std::endl;
    }
} catch (const std::exception& e) {
    std::cerr << "Failed to load model: " << e.what() << std::endl;
}
```

### 2. 加载 OBJ 文件

```cpp
// 加载 OBJ 文件（可能包含多个子网格）
auto meshes = ModelLoader::loadOBJ("assets/models/building.obj");

// 加载 OBJ 并翻转 UV 坐标（某些工具导出的 OBJ 需要）
auto meshes = ModelLoader::loadOBJ("assets/models/character.obj", true);
```

**OBJ 格式特性：**
- 支持多对象/组（`o` 和 `g` 关键字）
- 支持顶点位置 (`v`)、法线 (`vn`)、纹理坐标 (`vt`)
- 支持面索引格式：`f v/vt/vn`、`f v//vn`、`f v/vt`、`f v`
- 自动三角形化四边形和多边形

### 3. 加载 STL 文件

```cpp
// 加载 STL 文件（二进制或 ASCII，自动检测）
Mesh mesh = ModelLoader::loadSTL("assets/models/part.stl");

std::cout << "Loaded STL mesh: " << mesh.name << std::endl;
std::cout << "Triangles: " << mesh.indexCount / 3 << std::endl;
```

**STL 格式特性：**
- 自动检测二进制或 ASCII 格式
- 提供顶点位置和法线
- 默认设置白色顶点颜色
- 纹理坐标为 (0, 0)

### 4. 检测文件格式

```cpp
auto format = ModelLoader::detectFormat("model.fbx");

switch (format) {
    case ModelLoader::ModelFormat::OBJ:
        std::cout << "Wavefront OBJ file" << std::endl;
        break;
    case ModelLoader::ModelFormat::STL:
        std::cout << "STL file" << std::endl;
        break;
    case ModelLoader::ModelFormat::Unknown:
        std::cout << "Unknown format" << std::endl;
        break;
    default:
        std::cout << "Format not yet supported" << std::endl;
}
```

## 完整示例：加载并创建 GPU 缓冲区

```cpp
#include "RenderCore/ResourceManager/public/ResourceManagerUtils.hpp"
#include "RenderCore/VulkanCore/public/VKResource.hpp"

void loadModelToGPU(const std::string& filePath) {
    // 1. 加载模型文件
    auto meshes = ModelLoader::loadModel(filePath);
    
    for (auto& mesh : meshes) {
        // 2. 准备顶点数据（假设你有 vertices 和 indices 数组）
        // 注意：当前实现只返回元数据，你需要在 loadOBJ/loadSTL 中保存实际数据
        
        // 3. 创建顶点缓冲区
        vkcore::BufferDesc vertexBufferDesc;
        vertexBufferDesc.size = mesh.vertexCount * sizeof(Vertex);
        vertexBufferDesc.usageFlags = vk::BufferUsageFlagBits::eVertexBuffer | 
                                      vk::BufferUsageFlagBits::eTransferDst;
        vertexBufferDesc.memoryUsage = VMA_MEMORY_USAGE_GPU_ONLY;
        
        mesh.vertexBuffer = std::make_shared<vkcore::Buffer>(
            device, allocator, vertexBufferDesc);
        
        // 4. 创建索引缓冲区
        vkcore::BufferDesc indexBufferDesc;
        indexBufferDesc.size = mesh.indexCount * sizeof(uint32_t);
        indexBufferDesc.usageFlags = vk::BufferUsageFlagBits::eIndexBuffer | 
                                     vk::BufferUsageFlagBits::eTransferDst;
        indexBufferDesc.memoryUsage = VMA_MEMORY_USAGE_GPU_ONLY;
        
        mesh.indexBuffer = std::make_shared<vkcore::Buffer>(
            device, allocator, indexBufferDesc);
        
        // 5. 上传数据到 GPU（使用 staging buffer）
        // ... 省略上传代码 ...
        
        std::cout << "Loaded mesh: " << mesh.name << std::endl;
    }
}
```

## 注意事项

### 当前限制

1. **数据未包含在返回的 Mesh 中**：
   - 当前实现只解析文件并返回元数据（顶点数量、索引数量）
   - 实际顶点和索引数据需要在 `loadOBJ()` 和 `loadSTL()` 中暴露出来
   - 建议修改返回类型或在 Mesh 结构中添加临时数据容器

2. **需要手动创建 GPU 缓冲区**：
   - `Mesh` 结构中的 `vertexBuffer` 和 `indexBuffer` 在加载后为空
   - 需要调用者使用 `vkcore::Buffer` 创建并上传数据

3. **部分格式未实现**：
   - FBX、PLY、glTF 需要第三方库（Assimp 或 tinygltf）
   - 可以通过 vcpkg 安装：`vcpkg install assimp`

### 推荐改进

```cpp
// 建议在 Mesh 结构中添加 CPU 端数据
struct Mesh
{
    std::string name;
    std::shared_ptr<vkcore::Buffer> vertexBuffer;
    std::shared_ptr<vkcore::Buffer> indexBuffer;
    uint32_t vertexCount{0};
    uint32_t indexCount{0};
    
    // 添加 CPU 端数据（可选）
    std::vector<Vertex> cpuVertices;  // 加载时填充
    std::vector<uint32_t> cpuIndices; // 加载时填充
};
```

## 错误处理

所有加载函数都会在失败时抛出 `std::runtime_error`：

```cpp
try {
    auto meshes = ModelLoader::loadModel("model.obj");
} catch (const std::runtime_error& e) {
    // 可能的错误：
    // - "Material JSON file not found: ..."
    // - "Failed to open OBJ file: ..."
    // - "No geometry found in OBJ file: ..."
    // - "Unsupported or unknown model format: ..."
    std::cerr << "Error: " << e.what() << std::endl;
}
```

## 性能建议

1. **异步加载**：对于大型模型，考虑在后台线程中加载
2. **顶点去重**：OBJ 加载器当前不去重顶点，可以添加哈希表优化
3. **内存映射**：大文件可以使用内存映射 I/O
4. **进度回调**：可以添加回调参数报告加载进度

## 未来扩展

- [ ] 添加材质信息解析（MTL 文件）
- [ ] 集成 Assimp 支持更多格式
- [ ] 添加顶点去重和索引优化
- [ ] 支持骨骼动画数据
- [ ] 添加包围盒自动计算
- [ ] 支持 LOD 级别生成
