# RenderGraph 信号量同步优化说明

## 概述
已完成对 RenderGraph 的信号量同步优化，移除了性能低下的 `waitIdle()` 调用，实现了正确的 GPU 同步机制。

## 主要改进

### 1. 新增同步原语封装

**文件**: `RDGSyncInfo.hpp` / `RDGSyncInfo.cpp`

添加了以下新类型：

#### `RDGWaitInfo`
封装单个等待信号量的信息：
```cpp
struct RDGWaitInfo {
    vk::Semaphore semaphore;           // 要等待的信号量
    vk::PipelineStageFlags waitStage;  // 等待的管线阶段
};
```

#### `RDGSyncInfo`
封装完整的同步信息：
```cpp
struct RDGSyncInfo {
    std::vector<RDGWaitInfo> waitSemaphores;      // 需要等待的信号量列表
    std::vector<vk::Semaphore> signalSemaphores;  // 执行完成后触发的信号量列表
    std::optional<vk::Fence> executionFence;      // 执行完成的 Fence（可选）
    
    // 便利函数
    void addWaitSemaphore(vk::Semaphore, vk::PipelineStageFlags);
    void addSignalSemaphore(vk::Semaphore);
    void setExecutionFence(vk::Fence);
};
```

#### `RDGFrameSyncManager`
管理多帧并行（Frames in Flight）的同步：
```cpp
class RDGFrameSyncManager {
public:
    RDGFrameSyncManager(vk::Device device, size_t maxFramesInFlight = 2);
    
    RDGSyncInfo& getCurrentFrameSync();
    void advanceFrame();  // 前进到下一帧，自动等待 Fence
    void waitAll();       // 等待所有帧完成
    
    std::pair<vk::Semaphore, vk::Semaphore> getSwapChainSemaphores(size_t frameIndex);
};
```

### 2. 修改 RenderGraph::execute()

**之前**:
```cpp
void RenderGraph::execute() {
    // ... 录制命令 ...
    queue.submit(submitInfo);
    m_device.get().waitIdle();  // ❌ 阻塞 CPU，破坏性能
}
```

**之后**:
```cpp
void RenderGraph::execute(RDGSyncInfo* syncInfo = nullptr) {
    // ... 录制命令 ...
    
    // 设置等待信号量
    if (syncInfo && !syncInfo->waitSemaphores.empty()) {
        for (const auto& wait : syncInfo->waitSemaphores) {
            waitSemaphores.push_back(wait.semaphore);
            waitStages.push_back(wait.waitStage);
        }
        submitInfo.pWaitSemaphores = waitSemaphores.data();
        submitInfo.pWaitDstStageMask = waitStages.data();
    }
    
    // 设置触发信号量
    if (syncInfo && !syncInfo->signalSemaphores.empty()) {
        submitInfo.pSignalSemaphores = syncInfo->signalSemaphores.data();
    }
    
    // 获取 Fence
    vk::Fence fence = (syncInfo && syncInfo->executionFence) 
                      ? syncInfo->executionFence.value() 
                      : vk::Fence{};
    
    // 异步提交，不阻塞 CPU ✅
    queue.submit(submitInfo, fence);
    
    // 不再调用 waitIdle()！
}
```

### 3. 更新 RDGBuilder 接口

```cpp
// 旧接口（仍然支持，用于向后兼容）
void execute();

// 新接口（推荐使用）
void execute(RDGSyncInfo* syncInfo);
```

## 使用示例

### 示例 1: 基础使用（带 Fence）

```cpp
// 创建 Fence
vk::Fence fence = device.createFence({vk::FenceCreateFlagBits::eSignaled});

// 设置同步信息
RDGSyncInfo syncInfo;
syncInfo.setExecutionFence(fence);

// 执行渲染图
RDGBuilder builder(device, cmdManager, allocator);
// ... 添加 passes ...
builder.execute(&syncInfo);

// 后续可以检查或等待 Fence
device.waitForFences(fence, VK_TRUE, UINT64_MAX);
```

### 示例 2: SwapChain 集成

```cpp
// 获取 SwapChain 图像
uint32_t imageIndex;
vk::Semaphore imageAvailable = ...;  // SwapChain acquire 时获得
vk::Semaphore renderFinished = ...;  // Present 前需要等待
vk::Fence fence = ...;

// 设置同步信息
RDGSyncInfo syncInfo;
syncInfo.addWaitSemaphore(imageAvailable, vk::PipelineStageFlagBits::eColorAttachmentOutput);
syncInfo.addSignalSemaphore(renderFinished);
syncInfo.setExecutionFence(fence);

// 构建渲染图
RDGBuilder builder(device, cmdManager, allocator);

// 导入 SwapChain 图像
auto swapChainImg = builder.getSwapChainAttachment(swapChain, imageIndex);

// 添加渲染 Pass
builder.addPass("MainPass", [](vk::CommandBuffer cmd) {
    // ... 渲染命令 ...
}).writeColorAttachment(swapChainImg);

// 执行（异步）
builder.execute(&syncInfo);

// Present（会等待 renderFinished 信号量）
swapChain.present(imageIndex, renderFinished);
```

### 示例 3: 多帧并行（推荐生产环境使用）

```cpp
constexpr size_t MAX_FRAMES_IN_FLIGHT = 2;

// 创建帧同步管理器
RDGFrameSyncManager frameSyncManager(device.get(), MAX_FRAMES_IN_FLIGHT);

// 渲染循环
while (!shouldClose) {
    // 等待当前帧完成（自动）
    frameSyncManager.advanceFrame();
    
    // 获取当前帧的同步信息
    RDGSyncInfo& syncInfo = frameSyncManager.getCurrentFrameSync();
    
    // 获取 SwapChain 图像
    uint32_t imageIndex;
    auto [imageAvailable, renderFinished] = 
        frameSyncManager.getSwapChainSemaphores(frameSyncManager.getCurrentFrameIndex());
    
    swapChain.acquireNextImage(&imageIndex, imageAvailable);
    
    // 设置等待/触发信号量
    syncInfo.clear();
    syncInfo.addWaitSemaphore(imageAvailable, vk::PipelineStageFlagBits::eColorAttachmentOutput);
    syncInfo.addSignalSemaphore(renderFinished);
    
    // 构建并执行渲染图
    RDGBuilder builder(device, cmdManager, allocator);
    auto swapChainImg = builder.getSwapChainAttachment(swapChain, imageIndex);
    
    builder.addPass("MainPass", [&](vk::CommandBuffer cmd) {
        // ... 渲染命令 ...
    }).writeColorAttachment(swapChainImg);
    
    builder.execute(&syncInfo);
    
    // Present
    swapChain.present(imageIndex, renderFinished);
}

// 关闭前等待所有帧完成
frameSyncManager.waitAll();
```

## 性能对比

### 之前（使用 waitIdle）
- ❌ CPU 阻塞直到 GPU 完全空闲
- ❌ 无法实现多帧并行
- ❌ CPU-GPU 利用率低
- ❌ 帧率受限于单帧执行时间

### 之后（使用 Fence/Semaphore）
- ✅ CPU 和 GPU 异步并行
- ✅ 支持多帧并行（2-3帧）
- ✅ 更高的 CPU-GPU 利用率
- ✅ 帧率可显著提升（理论上 2-3 倍）

## 注意事项

1. **向后兼容**: 旧代码仍然可以调用 `execute()` 不带参数，但不推荐用于生产环境

2. **资源生命周期**: 使用多帧并行时，需要确保资源在多帧之间正确管理：
   - 瞬态资源由 RenderGraph 自动管理
   - 外部资源（如 Uniform Buffer）需要每帧独立的副本

3. **SwapChain 同步**: 必须正确使用 `imageAvailable` 和 `renderFinished` 信号量

4. **调试**: 如果需要同步等待以便调试，可以在 `execute()` 后手动调用：
   ```cpp
   builder.execute(&syncInfo);
   if (syncInfo.executionFence) {
       device.waitForFences(syncInfo.executionFence.value(), VK_TRUE, UINT64_MAX);
   }
   ```

## 未来改进方向

1. **异步计算队列**: 支持 Compute Queue 的信号量同步
2. **异步传输队列**: 支持 Transfer Queue 的异步资源上传
3. **时间线信号量**: 使用 Vulkan 1.2 的 Timeline Semaphore 简化同步
4. **GPU 资源追踪**: 自动追踪资源的 GPU 使用状态，避免手动管理

## 相关文件

- `src/RenderCore/RenderGraph/public/RDGSyncInfo.hpp`
- `src/RenderCore/RenderGraph/private/RDGSyncInfo.cpp`
- `src/RenderCore/RenderGraph/private/RenderGraph.hpp`
- `src/RenderCore/RenderGraph/private/RenderGraph.cpp`
- `src/RenderCore/RenderGraph/public/RDGBuilder.hpp`
- `src/RenderCore/RenderGraph/private/RDGBuilder.cpp`
