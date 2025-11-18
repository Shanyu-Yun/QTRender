模块结构
在 src/RenderCore/ 目录下创建一个新模块 RenderGraph：

RenderCore/
├── RenderGraph/
│   ├── public/
│   │   ├── RDGBuilder.hpp     (用户的主要接口)
│   │   ├── RDGHandle.hpp      (虚拟资源句柄定义)
│   │   └── RDGPass.hpp        (渲染通道的声明)
│   ├── private/
│   │   ├── RenderGraph.hpp    (RDG "后端" 编译器)
│   │   ├── RenderGraph.cpp
│   │   ├── RDGPass.cpp
│   │   ├── RDGBuilder.cpp
│   │   └── RDGResource.hpp    (内部资源表示)
│   └── CMakeLists.txt         (新模块的CMake配置)
├── ResourceManager/
├── Scene/
└── VulkanCore/
1. RDGHandle.hpp (虚拟资源句柄)
这是 RDG 的核心抽象。这些只是轻量级的ID，用于在“声明”阶段代表一个资源，它们不占用 GPU 显存。

C++

// src/RenderCore/RenderGraph/public/RDGHandle.hpp
#pragma once
#include "RenderCore/VulkanCore/public/VKResource.hpp" // 为了 vkcore::ImageDesc等
#include <cstdint>

namespace rendercore
{

// 通用句柄，内部实现为索引
enum class RDGHandleType : uint8_t { Texture, Buffer };
using RDGResourceHandle = uint32_t;
constexpr RDGResourceHandle kInvalidHandle = 0;

// 类型安全的句柄 (防止将Buffer传给需要Texture的函数)
struct RDGTextureHandle {
    RDGResourceHandle handle = kInvalidHandle;
    bool isValid() const { return handle != kInvalidHandle; }
    friend bool operator==(const RDGTextureHandle& a, const RDGTextureHandle& b) { return a.handle == b.handle; }
};

struct RDGBufferHandle {
    RDGResourceHandle handle = kInvalidHandle;
    bool isValid() const { return handle != kInvalidHandle; }
    friend bool operator==(const RDGBufferHandle& a, const RDGBufferHandle& b) { return a.handle == b.handle; }
};

// 瞬态资源的描述 (用于创建)
// 注意：这些是对 vkcore::ImageDesc 和 vkcore::BufferDesc 的简化
struct RDGTextureDesc {
    std::string name;
    vk::Format format;
    vk::Extent3D extent;
    vk::ImageUsageFlags usage;
    // ... 其他需要的参数 (mipLevels, layers, samples)
};

struct RDGBufferDesc {
    std::string name;
    vk::DeviceSize size;
    vk::BufferUsageFlags usage;
    // ...
};

} // namespace rendercore
2. RDGPass.hpp (渲染通道)
RDGPass 封装了一个渲染作业单元（Graphics 或 Compute）。它声明了它将如何使用哪些资源（读、写、作为颜色附件、作为深度附件）。

C++

// src/RenderCore/RenderGraph/public/RDGPass.hpp
#pragma once
#include "RDGHandle.hpp"
#include <functional>
#include <string>
#include <vector>
#include <vulkan/vulkan.hpp>

namespace vkcore { class CommandBufferHandle; } // 前向声明

namespace rendercore
{

class RDGBuilder; // 前向声明

/**
 * @class RDGPass
 * @brief 声明一个渲染通道及其资源依赖
 * @details 通过 RDGBuilder::addPass() 创建
 */
class RDGPass
{
public:
    // Pass 的具体执行逻辑 (录制Vulkan命令)
    using ExecuteCallback = std::function<void(vk::CommandBuffer cmd)>;

    // 声明读依赖
    RDGPass& readTexture(RDGTextureHandle handle, vk::PipelineStageFlags stages);
    RDGPass& readBuffer(RDGBufferHandle handle, vk::PipelineStageFlags stages);
    
    // 声明颜色写依赖 (映射到 Vulkan 1.3 动态渲染)
    RDGPass& writeColorAttachment(RDGTextureHandle handle, 
                                  vk::AttachmentLoadOp loadOp, 
                                  vk::AttachmentStoreOp storeOp, 
                                  vk::ClearColorValue clearValue = {});
    
    // 声明深度写依赖
    RDGPass& writeDepthStencilAttachment(RDGTextureHandle handle, 
                                         vk::AttachmentLoadOp loadOp, 
                                         vk::AttachmentStoreOp storeOp, 
                                         vk::ClearDepthStencilValue clearValue = {});

    // (未来可添加：writeStorageTexture, writeStorageBuffer, ... )

    const std::string& getName() const { return m_name; }

private:
    // 只能由 RDGBuilder 创建
    friend class RDGBuilder;
    friend class RenderGraph; // 允许内部"编译器"访问

    RDGPass(std::string name, ExecuteCallback&& callback);

    std::string m_name;
    ExecuteCallback m_executeCallback;

    // 存储依赖关系
    struct TextureReadAccess {
        RDGTextureHandle handle;
        vk::PipelineStageFlags stages;
        // ...
    };
    std::vector<TextureReadAccess> m_textureReads;
    // ... 其他 m_bufferReads, m_colorAttachments, m_depthAttachment ...
};

} // namespace rendercore
3. RDGBuilder.hpp (公开的图构建器)
这是您每帧都会使用的主要接口。它负责“记录”您的渲染意图。

C++

// src/RenderCore/RenderGraph/public/RDGBuilder.hpp
#pragma once
#include "RDGHandle.hpp"
#include "RDGPass.hpp"
#include "RenderCore/VulkanCore/public/Device.hpp"
#include <memory>

namespace vkcore { 
    class CommandPoolManager; 
    class SwapChain;
    class Image;
    class Buffer;
}
namespace vma { class Allocator; }

namespace rendercore
{

// 前向声明内部实现
class RenderGraph;

/**
 * @class RDGBuilder
 * @brief 每帧用于声明渲染图的前端接口
 */
class RDGBuilder
{
public:
    RDGBuilder(vkcore::Device& device, 
               vkcore::CommandPoolManager& cmdManager, 
               VmaAllocator allocator);
    ~RDGBuilder(); // 析构函数中会处理 execute (如果未显式调用)

    /**
     * @brief 添加一个渲染通道
     * @param name 通道的调试名称
     * @param callback 包含Vulkan命令录制的Lambda函数
     * @return RDGPass& 通道对象的引用，用于链式声明依赖
     */
    RDGPass& addPass(std::string name, RDGPass::ExecuteCallback&& callback);

    /**
     * @brief 创建一个瞬态纹理 (例如 G-Buffer, 深度图)
     * @param desc 纹理描述
     * @return RDGTextureHandle 虚拟句柄
     */
    RDGTextureHandle createTexture(const RDGTextureDesc& desc);
    
    /**
     * @brief 创建一个瞬态缓冲区
     */
    RDGBufferHandle createBuffer(const RDGBufferDesc& desc);

    /**
     * @brief 导入一个外部(持久化)纹理到图中
     * @param image 指向由ResourceManager或SwapChain管理的 vkcore::Image
     * @return RDGTextureHandle 虚拟句柄
     */
    RDGTextureHandle registerExternalTexture(vkcore::Image* image);

    /**
     * @brief 导入一个外部(持久化)缓冲区到图中
     * @param buffer 指向由ResourceManager管理的 vkcore::Buffer
     * @return RDGBufferHandle 虚拟句柄
     */
    RDGBufferHandle registerExternalBuffer(vkcore::Buffer* buffer);
    
    /**
     * @brief 辅助函数：快速导入交换链后台缓冲区
     * @param swapChain 交换链
     * @param imageIndex 当前图像索引
     * @return RDGTextureHandle 虚拟句柄
     */
    RDGTextureHandle getSwapChainAttachment(vkcore::SwapChain& swapChain, uint32_t imageIndex);

    /**
     * @brief 编译图，执行所有优化，并提交到GPU
     */
    void execute();

private:
    // 指向PImpl (Pointer to implementation)
    // 复杂的编译逻辑被隐藏在 RenderGraph 类中
    std::unique_ptr<RenderGraph> m_pimpl;
};

} // namespace rendercore
4. 内部实现 RenderGraph.hpp/cpp (简要)
RDGBuilder 会持有一个 RenderGraph 类的实例 (m_pimpl)。这个内部类是 RDG 的“后端”或“编译器”，负责所有复杂工作：

资源注册：

registerExternalTexture：将 vkcore::Image* 指针与一个新的 RDGTextureHandle 关联，并存储其当前布局（例如 vk::ImageLayout::ePresentSrcKHR）。

createTexture：只存储 RDGTextureDesc 和一个新的 RDGTextureHandle。此时不分配任何内存。

execute() 阶段 1：编译（Compile）

构建依赖图：遍历所有 RDGPass 和它们的 m_reads / m_writes 列表，构建一个有向无环图（DAG）。

剔除 Pass (Culling)：从一个“最终输出”Pass（例如写入交换链的 Pass）开始，反向遍历图。任何无法到达该 Pass 的节点（例如，一个计算了阴影贴图但无人读取的 Pass）都将被标记为非活动状态，不会执行。

解析资源生命周期：遍历所有活动 Pass，确定每个瞬态资源（createTexture 创建的）的第一个使用 Pass 和最后一个使用 Pass。

execute() 阶段 2：分配（Allocate）

瞬态资源分配：遍历所有需要分配的瞬态资源。

内存复用（Aliasing）：(这是核心优化点) 查找一个已分配的、生命周期已结束的物理资源（vkcore::Image），如果其格式、大小、用法与新资源兼容，就复用这块显存，而不是创建新的。如果找不到，才从 VmaAllocator 创建一个新的 vkcore::Image。

句柄映射：将所有 RDGTextureHandle 映射到实际的 vkcore::Image*。

execute() 阶段 3：执行（Execute）

从 CommandPoolManager 获取一个命令缓冲区。

拓扑排序：按依赖顺序遍历所有活动 Pass。

对于每个 Pass：

屏障计算（Barrier Pass）：

查看该 Pass 的所有读/写依赖。

对于每个资源 R，查询它在上一个 Pass 使用后的旧布局（oldLayout）。

确定它在这个 Pass 中需要的新布局（newLayout）（例如，writeColorAttachment -> eColorAttachmentOptimal；readTexture -> eShaderReadOnlyOptimal）。

如果 oldLayout != newLayout，则自动插入一个 vk::ImageMemoryBarrier。

开始渲染：如果这是一个图形 Pass（有颜色或深度附件），则自动填充 vk::RenderingInfo（使用 RDGPass 中设置的 loadOp/storeOp）并调用 cmd.beginRendering(...)。

执行 Pass：调用 pass->m_executeCallback(cmd)，执行用户提供的绘制命令。

结束渲染：如果 beginRendering 被调用，则自动调用 cmd.endRendering()。

提交：结束命令缓冲区并将其提交到队列。

清理：释放本帧分配的所有瞬态资源（vkcore::Image 等）回池中。

5. 如何重构 MeshRenderer (集成 RDG)
您 main.cpp 中的 MeshRenderer 将变得极其简洁。

重构前的 renderFrame：

手动 acquireNextImage。

手动 waitForFences 和 resetFences。

手动 cmd->reset(), cmd->begin(), cmd->end(), submit(), present()。

recordCommandBuffer 内部：

手动 pipelineBarrier (Undefined -> Color)

手动 beginRendering

(绘制)

手动 endRendering

手动 pipelineBarrier (Color -> Present)