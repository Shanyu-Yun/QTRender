/**
 * @file RenderGraph.cpp
 * @brief RenderGraph类的实现 - RDG后端编译器和执行器
 */

#include "RenderGraph.hpp"
#include "VulkanCore/public/CommandPoolManager.hpp"
#include "VulkanCore/public/Device.hpp"
#include "VulkanCore/public/SwapChain.hpp"
#include "VulkanCore/public/VKResource.hpp"
#include <algorithm>
#include <iostream>
#include <stdexcept>

namespace rendercore
{

// ==================== RDGResource实现 ====================

// RDGTextureResource构造函数
RDGTextureResource::RDGTextureResource(RDGResourceHandle handle, const RDGTextureDesc &desc, RDGResourceType type)
    : m_handle(handle), m_desc(desc), m_type(type)
{
}

RDGTextureResource::RDGTextureResource(RDGResourceHandle handle, vkcore::Image *externalImage, const std::string &name,
                                       vk::ImageLayout currentLayout)
    : m_handle(handle), m_type(RDGResourceType::External), m_state(RDGResourceState::Allocated),
      m_physicalImage(externalImage), m_currentLayout(currentLayout)
{
    // 为外部资源创建描述符
    m_desc.name = name;
    m_desc.format = externalImage->getFormat();
    m_desc.extent = externalImage->getExtent();
    m_desc.usage = externalImage->getUsage();
    m_desc.mipLevels = externalImage->getMipLevels();
    m_desc.arrayLayers = externalImage->getArrayLayers();
}

bool RDGTextureResource::canAliasWith(const RDGTextureResource &other) const
{
    if (m_type != RDGResourceType::Transient || other.m_type != RDGResourceType::Transient)
    {
        return false; // 只有瞬态资源可以复用
    }

    if (m_lifetime.overlapsWith(other.m_lifetime))
    {
        return false; // 生命周期重叠，不能复用
    }

    // 检查格式和大小兼容性
    return m_desc.format == other.m_desc.format && m_desc.extent.width == other.m_desc.extent.width &&
           m_desc.extent.height == other.m_desc.extent.height && m_desc.extent.depth == other.m_desc.extent.depth &&
           m_desc.usage == other.m_desc.usage && m_desc.mipLevels == other.m_desc.mipLevels &&
           m_desc.arrayLayers == other.m_desc.arrayLayers;
}

// RDGBufferResource构造函数
RDGBufferResource::RDGBufferResource(RDGResourceHandle handle, const RDGBufferDesc &desc, RDGResourceType type)
    : m_handle(handle), m_desc(desc), m_type(type)
{
}

RDGBufferResource::RDGBufferResource(RDGResourceHandle handle, vkcore::Buffer *externalBuffer, const std::string &name)
    : m_handle(handle), m_type(RDGResourceType::External), m_state(RDGResourceState::Allocated),
      m_physicalBuffer(externalBuffer)
{
    // 为外部资源创建描述符
    m_desc.name = name;
    m_desc.size = externalBuffer->getSize();
    m_desc.usage = externalBuffer->getUsage();
}

bool RDGBufferResource::canAliasWith(const RDGBufferResource &other) const
{
    if (m_type != RDGResourceType::Transient || other.m_type != RDGResourceType::Transient)
    {
        return false;
    }

    if (m_lifetime.overlapsWith(other.m_lifetime))
    {
        return false;
    }

    return m_desc.size == other.m_desc.size && m_desc.usage == other.m_desc.usage;
}

// ==================== RenderGraph构造函数和析构函数 ====================

RenderGraph::RenderGraph(vkcore::Device &device, vkcore::CommandPoolManager &cmdManager, VmaAllocator allocator)
    : m_device(device), m_commandManager(cmdManager), m_allocator(allocator)
{
}

RenderGraph::~RenderGraph()
{
    // 销毁采样器
    destroySamplers();

    // 清理所有资源
    m_frameTextures.clear();
    m_frameBuffers.clear();
    m_texturePool.clear();
    m_bufferPool.clear();
}

// ==================== 资源管理接口 ====================

RDGTextureHandle RenderGraph::createTransientTexture(const RDGTextureDesc &desc)
{
    RDGResourceHandle handle = generateNextHandle();
    auto resource = std::make_unique<RDGTextureResource>(handle, desc, RDGResourceType::Transient);

    RDGTextureHandle textureHandle{handle};
    m_textureResources[handle] = std::move(resource);

    return textureHandle;
}

RDGBufferHandle RenderGraph::createTransientBuffer(const RDGBufferDesc &desc)
{
    RDGResourceHandle handle = generateNextHandle();
    auto resource = std::make_unique<RDGBufferResource>(handle, desc, RDGResourceType::Transient);

    RDGBufferHandle bufferHandle{handle};
    m_bufferResources[handle] = std::move(resource);

    return bufferHandle;
}

RDGTextureHandle RenderGraph::registerExternalTexture(vkcore::Image *image, const std::string &name,
                                                      vk::ImageLayout currentLayout)
{
    RDGResourceHandle handle = generateNextHandle();
    auto resource = std::make_unique<RDGTextureResource>(handle, image, name, currentLayout);

    RDGTextureHandle textureHandle{handle};
    m_textureResources[handle] = std::move(resource);

    // 记录当前布局
    m_textureLayouts[handle] = currentLayout;

    return textureHandle;
}

RDGBufferHandle RenderGraph::registerExternalBuffer(vkcore::Buffer *buffer, const std::string &name)
{
    RDGResourceHandle handle = generateNextHandle();
    auto resource = std::make_unique<RDGBufferResource>(handle, buffer, name);

    RDGBufferHandle bufferHandle{handle};
    m_bufferResources[handle] = std::move(resource);

    return bufferHandle;
}

RDGTextureHandle RenderGraph::importSwapChainImage(vkcore::SwapChain &swapChain, uint32_t imageIndex)
{
    // 创建一个临时的RDGTextureDesc来描述交换链图像
    RDGTextureDesc desc{};
    desc.name = "SwapChainImage_" + std::to_string(imageIndex);
    desc.format = swapChain.getSwapchainFormat();

    // 将vk::Extent2D转换为vk::Extent3D
    auto extent2D = swapChain.getSwapchainExtent();
    desc.extent = vk::Extent3D{extent2D.width, extent2D.height, 1};

    desc.usage = vk::ImageUsageFlagBits::eColorAttachment; // 交换链图像通常用作颜色附件
    desc.mipLevels = 1;
    desc.arrayLayers = 1;
    desc.samples = vk::SampleCountFlagBits::e1;
    desc.tiling = vk::ImageTiling::eOptimal;

    // 创建一个特殊的RDGTextureResource来表示SwapChain图像
    RDGResourceHandle handle = generateNextHandle();
    auto resource = std::make_unique<RDGTextureResource>(handle, desc, RDGResourceType::External);

    // 设置资源状态和特殊标记
    resource->setState(RDGResourceState::Allocated);
    resource->setSwapChainImageIndex(imageIndex); // 记住这是哪个SwapChain图像

    RDGTextureHandle textureHandle{handle};
    m_textureResources[handle] = std::move(resource);

    // 存储SwapChain引用以供后续使用
    m_swapChainMapping[handle] = &swapChain;

    // 记录当前布局（交换链图像通常开始时是undefined）
    m_textureLayouts[handle] = vk::ImageLayout::eUndefined;

    return textureHandle;
}

// ==================== Pass管理接口 ====================

RDGPass &RenderGraph::addPass(std::string name, RDGPass::ExecuteCallback &&callback)
{
    auto pass = std::make_unique<RDGPass>(std::move(name), std::move(callback));
    RDGPass &passRef = *pass;
    m_passes.push_back(std::move(pass));

    return passRef;
}

RDGPass &RenderGraph::addPassEx(std::string name, RDGPass::ExecuteCallbackEx &&callback)
{
    auto pass = std::make_unique<RDGPass>(std::move(name), std::move(callback));
    RDGPass &passRef = *pass;
    m_passes.push_back(std::move(pass));

    return passRef;
}

// ==================== 查询接口 ====================

size_t RenderGraph::getTransientResourceCount() const
{
    size_t count = 0;

    for (const auto &[handle, resource] : m_textureResources)
    {
        if (resource->isTransient())
        {
            count++;
        }
    }

    for (const auto &[handle, resource] : m_bufferResources)
    {
        if (resource->isTransient())
        {
            count++;
        }
    }

    return count;
}

// ==================== 编译和执行接口 ====================

void RenderGraph::compile()
{
    if (m_compiled)
    {
        throw std::runtime_error("RenderGraph::compile: Already compiled");
    }

    std::cout << "\n=== RenderGraph编译开始 ===" << std::endl;
    std::cout << "Pass数量: " << m_passes.size() << std::endl;
    std::cout << "瞬态资源数量: " << getTransientResourceCount() << std::endl;

    try
    {
        // 编译阶段1：构建依赖图
        buildDependencyGraph();

        // 编译阶段2：剔除无用Pass
        cullUnusedPasses();

        // 编译阶段3：分析资源生命周期
        analyzeResourceLifetime();

        // 编译阶段4：验证资源状态
        validateResourceStates();

        // 编译阶段5：计算屏障
        computeBarriers();

        m_compiled = true;

        // 输出编译统计
        size_t activePasses = 0;
        for (const auto &pass : m_compiledPasses)
        {
            if (pass->isActive())
            {
                activePasses++;
            }
        }

        std::cout << "活跃Pass数量: " << activePasses << std::endl;
        std::cout << "=== RenderGraph编译完成 ===" << std::endl;
    }
    catch (const std::exception &e)
    {
        std::cerr << "RenderGraph编译失败: " << e.what() << std::endl;
        throw;
    }
}

void RenderGraph::execute(RDGSyncInfo *syncInfo)
{
    if (!m_compiled)
    {
        throw std::runtime_error("RenderGraph::execute: Must compile before execute");
    }

    if (m_executed)
    {
        throw std::runtime_error("RenderGraph::execute: Already executed");
    }

    std::cout << "\n=== RenderGraph执行开始 ===" << std::endl;

    try
    {
        // 执行阶段1：分配物理资源
        allocateResources();

        // 执行阶段2：获取命令缓冲区并录制
        std::cout << "执行渲染图Pass..." << std::endl;

        // 获取命令缓冲区（使用CommandPoolManager）
        auto cmdBufferHandle = m_commandManager.allocate(vk::CommandBufferLevel::ePrimary);
        vk::CommandBuffer cmdBuffer = *cmdBufferHandle;

        // 开始录制命令缓冲区
        vk::CommandBufferBeginInfo beginInfo{};
        beginInfo.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit;
        cmdBuffer.begin(beginInfo);

        // 执行所有活跃的Pass
        for (size_t passIndex = 0; passIndex < m_compiledPasses.size(); ++passIndex)
        {
            const auto &compiledPass = m_compiledPasses[passIndex];
            if (!compiledPass->isActive())
            {
                continue;
            }

            const RDGPass *originalPass = compiledPass->getOriginalPass();
            std::cout << "  执行Pass: " << originalPass->getName() << std::endl;

            // 执行屏障（在Pass开始前）
            const auto &barriers = compiledPass->getBarriers();
            if (!barriers.empty())
            {
                std::cout << "    执行 " << barriers.size() << " 个屏障" << std::endl;
                executeBarriers(cmdBuffer, barriers);
            }

            // 如果是图形Pass，设置渲染状态
            if (compiledPass->isGraphicsPass())
            {
                beginGraphicsPass(cmdBuffer, *originalPass);
            }

            // 执行Pass的回调函数
            try
            {
                if (originalPass->m_useExtendedCallback)
                {
                    // 使用扩展回调（带资源访问器）
                    if (originalPass->m_executeCallbackEx)
                    {
                        RDGResourceAccessor resourceAccessor(this);
                        originalPass->m_executeCallbackEx(cmdBuffer, resourceAccessor);
                    }
                }
                else
                {
                    // 使用简单回调
                    if (originalPass->m_executeCallback)
                    {
                        originalPass->m_executeCallback(cmdBuffer);
                    }
                }
            }
            catch (const std::exception &e)
            {
                std::cerr << "    Pass执行失败: " << e.what() << std::endl;
                // 继续执行其他Pass
            }

            // 如果是图形Pass，结束渲染
            if (compiledPass->isGraphicsPass())
            {
                endGraphicsPass(cmdBuffer);
            }
        }

        // 结束命令缓冲区录制
        cmdBuffer.end();

        // 准备提交信息
        vk::SubmitInfo submitInfo{};
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &cmdBuffer;

        // 设置等待信号量（如果提供）
        std::vector<vk::Semaphore> waitSemaphores;
        std::vector<vk::PipelineStageFlags> waitStages;
        if (syncInfo && !syncInfo->waitSemaphores.empty())
        {
            std::cout << "  等待 " << syncInfo->waitSemaphores.size() << " 个信号量" << std::endl;
            for (const auto &waitInfo : syncInfo->waitSemaphores)
            {
                waitSemaphores.push_back(waitInfo.semaphore);
                waitStages.push_back(waitInfo.waitStage);
            }
            submitInfo.waitSemaphoreCount = static_cast<uint32_t>(waitSemaphores.size());
            submitInfo.pWaitSemaphores = waitSemaphores.data();
            submitInfo.pWaitDstStageMask = waitStages.data();
        }

        // 设置触发信号量（如果提供）
        if (syncInfo && !syncInfo->signalSemaphores.empty())
        {
            std::cout << "  触发 " << syncInfo->signalSemaphores.size() << " 个信号量" << std::endl;
            submitInfo.signalSemaphoreCount = static_cast<uint32_t>(syncInfo->signalSemaphores.size());
            submitInfo.pSignalSemaphores = syncInfo->signalSemaphores.data();
        }

        // 获取 Fence（如果提供）
        vk::Fence fence = (syncInfo && syncInfo->executionFence) ? syncInfo->executionFence.value() : vk::Fence{};

        // 提交命令缓冲区到GPU（使用图形队列）
        vk::Queue graphicsQueue = m_device.getGraphicsQueue();

        try
        {
            graphicsQueue.submit(submitInfo, fence);
            std::cout << "  命令缓冲区已提交到GPU" << std::endl;

            if (fence)
            {
                std::cout << "  已设置执行 Fence 用于同步" << std::endl;
            }
        }
        catch (const vk::SystemError &e)
        {
            throw std::runtime_error("Failed to submit command buffer: " + std::string(e.what()));
        }

        // 注意：不再调用 waitIdle()！
        // 如果用户需要同步等待，应该通过 syncInfo 的 Fence 来实现
        // 例如：device.waitForFences(fence, VK_TRUE, UINT64_MAX);

        m_executed = true;

        std::cout << "=== RenderGraph执行完成（异步）===" << std::endl;
    }
    catch (const std::exception &e)
    {
        std::cerr << "RenderGraph执行失败: " << e.what() << std::endl;
        throw;
    }
}

// ==================== 内部编译阶段实现 ====================

void RenderGraph::buildDependencyGraph()
{
    std::cout << "构建依赖图..." << std::endl;

    // 创建编译后的Pass对象
    m_compiledPasses.clear();
    m_compiledPasses.reserve(m_passes.size());

    for (size_t i = 0; i < m_passes.size(); ++i)
    {
        auto compiledPass = std::make_unique<RDGCompiledPass>(*m_passes[i], static_cast<uint32_t>(i));
        m_compiledPasses.push_back(std::move(compiledPass));
    }

    std::cout << "依赖图构建完成" << std::endl;
}

void RenderGraph::cullUnusedPasses()
{
    std::cout << "剔除未使用的Pass..." << std::endl;

    // 实现反向依赖分析：从写入外部资源的Pass开始反向遍历
    std::vector<bool> reachable(m_compiledPasses.size(), false);
    std::vector<size_t> workList;

    // 第一步：找到所有写入外部资源的Pass作为根节点
    for (size_t i = 0; i < m_compiledPasses.size(); ++i)
    {
        const RDGPass *pass = m_compiledPasses[i]->getOriginalPass();
        bool writesExternalResource = false;

        // 检查颜色附件
        for (const auto &colorAttachment : pass->m_colorAttachments)
        {
            auto it = m_textureResources.find(colorAttachment.handle.handle);
            if (it != m_textureResources.end() && it->second->isExternal())
            {
                writesExternalResource = true;
                break;
            }
        }

        // 检查深度附件
        if (!writesExternalResource && pass->m_depthAttachment.handle.isValid())
        {
            auto it = m_textureResources.find(pass->m_depthAttachment.handle.handle);
            if (it != m_textureResources.end() && it->second->isExternal())
            {
                writesExternalResource = true;
            }
        }

        // 检查存储纹理写入
        if (!writesExternalResource)
        {
            for (const auto &textureWrite : pass->m_textureWrites)
            {
                auto it = m_textureResources.find(textureWrite.handle.handle);
                if (it != m_textureResources.end() && it->second->isExternal())
                {
                    writesExternalResource = true;
                    break;
                }
            }
        }

        // 检查存储缓冲区写入
        if (!writesExternalResource)
        {
            for (const auto &bufferWrite : pass->m_bufferWrites)
            {
                auto it = m_bufferResources.find(bufferWrite.handle.handle);
                if (it != m_bufferResources.end() && it->second->isExternal())
                {
                    writesExternalResource = true;
                    break;
                }
            }
        }

        if (writesExternalResource)
        {
            reachable[i] = true;
            workList.push_back(i);
            std::cout << "  根节点Pass: " << pass->getName() << std::endl;
        }
    }

    // 第二步：反向标记所有被根节点依赖的Pass
    while (!workList.empty())
    {
        size_t currentPassIndex = workList.back();
        workList.pop_back();

        const RDGPass *currentPass = m_compiledPasses[currentPassIndex]->getOriginalPass();

        // 收集当前Pass读取的所有资源
        std::vector<RDGResourceHandle> readResources;

        for (const auto &textureRead : currentPass->m_textureReads)
        {
            readResources.push_back(textureRead.handle.handle);
        }

        for (const auto &bufferRead : currentPass->m_bufferReads)
        {
            readResources.push_back(bufferRead.handle.handle);
        }

        // 查找写入这些资源的Pass
        for (size_t i = 0; i < currentPassIndex; ++i)
        {
            if (reachable[i])
            {
                continue; // 已经标记为可达
            }

            const RDGPass *pass = m_compiledPasses[i]->getOriginalPass();
            bool writesRequiredResource = false;

            // 检查是否写入了当前Pass需要读取的资源
            for (RDGResourceHandle requiredResource : readResources)
            {
                // 检查颜色附件
                for (const auto &colorAttachment : pass->m_colorAttachments)
                {
                    if (colorAttachment.handle.handle == requiredResource)
                    {
                        writesRequiredResource = true;
                        break;
                    }
                }

                // 检查深度附件
                if (!writesRequiredResource && pass->m_depthAttachment.handle.handle == requiredResource)
                {
                    writesRequiredResource = true;
                }

                // 检查存储纹理写入
                if (!writesRequiredResource)
                {
                    for (const auto &textureWrite : pass->m_textureWrites)
                    {
                        if (textureWrite.handle.handle == requiredResource)
                        {
                            writesRequiredResource = true;
                            break;
                        }
                    }
                }

                // 检查存储缓冲区写入
                if (!writesRequiredResource)
                {
                    for (const auto &bufferWrite : pass->m_bufferWrites)
                    {
                        if (bufferWrite.handle.handle == requiredResource)
                        {
                            writesRequiredResource = true;
                            break;
                        }
                    }
                }

                if (writesRequiredResource)
                {
                    break;
                }
            }

            if (writesRequiredResource)
            {
                reachable[i] = true;
                workList.push_back(i);
                std::cout << "  依赖Pass: " << pass->getName() << std::endl;
            }
        }
    }

    // 第三步：根据可达性标记Pass
    size_t activePasses = 0;
    size_t culledPasses = 0;

    for (size_t i = 0; i < m_compiledPasses.size(); ++i)
    {
        m_compiledPasses[i]->setActive(reachable[i]);
        if (reachable[i])
        {
            activePasses++;
        }
        else
        {
            culledPasses++;
            std::cout << "  剔除Pass: " << m_compiledPasses[i]->getOriginalPass()->getName() << std::endl;
        }
    }

    std::cout << "Pass剔除完成，活跃Pass: " << activePasses << "/" << m_compiledPasses.size()
              << "，剔除Pass: " << culledPasses << std::endl;
}

void RenderGraph::analyzeResourceLifetime()
{
    std::cout << "分析资源生命周期..." << std::endl;

    // 遍历所有活跃Pass，记录资源使用
    for (size_t passIndex = 0; passIndex < m_compiledPasses.size(); ++passIndex)
    {
        const auto &compiledPass = m_compiledPasses[passIndex];
        if (!compiledPass->isActive())
        {
            continue;
        }

        const RDGPass *pass = compiledPass->getOriginalPass();

        // 记录纹理读取
        for (const auto &textureRead : pass->m_textureReads)
        {
            auto it = m_textureResources.find(textureRead.handle.handle);
            if (it != m_textureResources.end())
            {
                it->second->updateLifetime(static_cast<uint32_t>(passIndex));
            }
        }

        // 记录纹理写入
        for (const auto &textureWrite : pass->m_textureWrites)
        {
            auto it = m_textureResources.find(textureWrite.handle.handle);
            if (it != m_textureResources.end())
            {
                it->second->updateLifetime(static_cast<uint32_t>(passIndex));
            }
        }

        // 记录颜色附件
        for (const auto &colorAttachment : pass->m_colorAttachments)
        {
            auto it = m_textureResources.find(colorAttachment.handle.handle);
            if (it != m_textureResources.end())
            {
                it->second->updateLifetime(static_cast<uint32_t>(passIndex));
            }
        }

        // 记录深度附件
        if (pass->m_depthAttachment.handle.isValid())
        {
            auto it = m_textureResources.find(pass->m_depthAttachment.handle.handle);
            if (it != m_textureResources.end())
            {
                it->second->updateLifetime(static_cast<uint32_t>(passIndex));
            }
        }

        // 记录缓冲区读取
        for (const auto &bufferRead : pass->m_bufferReads)
        {
            auto it = m_bufferResources.find(bufferRead.handle.handle);
            if (it != m_bufferResources.end())
            {
                it->second->updateLifetime(static_cast<uint32_t>(passIndex));
            }
        }

        // 记录缓冲区写入
        for (const auto &bufferWrite : pass->m_bufferWrites)
        {
            auto it = m_bufferResources.find(bufferWrite.handle.handle);
            if (it != m_bufferResources.end())
            {
                it->second->updateLifetime(static_cast<uint32_t>(passIndex));
            }
        }
    }

    std::cout << "资源生命周期分析完成" << std::endl;
}

void RenderGraph::allocateResources()
{
    std::cout << "分配物理资源..." << std::endl;

    // 清理上一帧的资源
    m_frameTextures.clear();
    m_frameBuffers.clear();

    // 分配瞬态纹理
    for (auto &[handle, resource] : m_textureResources)
    {
        if (resource->isTransient() && resource->isUsed())
        {
            vkcore::Image *physicalImage = allocateTransientTexture(*resource);
            resource->setPhysicalImage(physicalImage);
        }
    }

    // 分配瞬态缓冲区
    for (auto &[handle, resource] : m_bufferResources)
    {
        if (resource->isTransient() && resource->isUsed())
        {
            vkcore::Buffer *physicalBuffer = allocateTransientBuffer(*resource);
            resource->setPhysicalBuffer(physicalBuffer);
        }
    }

    std::cout << "物理资源分配完成" << std::endl;
}

void RenderGraph::computeBarriers()
{
    std::cout << "计算屏障..." << std::endl;

    // 跟踪每个资源的最后一次访问信息
    struct ResourceAccessInfo
    {
        vk::PipelineStageFlags lastStages = vk::PipelineStageFlagBits::eTopOfPipe;
        vk::AccessFlags lastAccess = vk::AccessFlagBits::eNone;
        bool wasWrite = false;
    };

    std::unordered_map<RDGResourceHandle, ResourceAccessInfo> textureAccessInfo;
    std::unordered_map<RDGResourceHandle, ResourceAccessInfo> bufferAccessInfo;

    // 遍历所有活跃Pass，计算所需的屏障
    for (size_t passIndex = 0; passIndex < m_compiledPasses.size(); ++passIndex)
    {
        auto &compiledPass = m_compiledPasses[passIndex];
        if (!compiledPass->isActive())
        {
            continue;
        }

        const RDGPass *pass = compiledPass->getOriginalPass();

        // 处理纹理读取屏障
        for (const auto &textureRead : pass->m_textureReads)
        {
            auto it = m_textureResources.find(textureRead.handle.handle);
            if (it == m_textureResources.end())
                continue;

            vk::ImageLayout requiredLayout = textureRead.layout;
            vk::ImageLayout currentLayout = m_textureLayouts[textureRead.handle.handle];

            auto &accessInfo = textureAccessInfo[textureRead.handle.handle];

            // 如果上一次是写入操作，需要WAR（Write-After-Read）屏障
            if (accessInfo.wasWrite)
            {
                addImageBarrier(*compiledPass, textureRead.handle, currentLayout, requiredLayout, accessInfo.lastAccess,
                                textureRead.access, accessInfo.lastStages, textureRead.stages);

                m_textureLayouts[textureRead.handle.handle] = requiredLayout;
            }
            else if (currentLayout != requiredLayout)
            {
                // 仅布局转换
                addImageBarrier(*compiledPass, textureRead.handle, currentLayout, requiredLayout,
                                vk::AccessFlagBits::eNone, textureRead.access, vk::PipelineStageFlagBits::eTopOfPipe,
                                textureRead.stages);

                m_textureLayouts[textureRead.handle.handle] = requiredLayout;
            }

            // 更新访问信息
            accessInfo.lastStages = textureRead.stages;
            accessInfo.lastAccess = textureRead.access;
            accessInfo.wasWrite = false;
        }

        // 处理颜色附件屏障（写入操作）
        for (const auto &colorAttachment : pass->m_colorAttachments)
        {
            auto it = m_textureResources.find(colorAttachment.handle.handle);
            if (it == m_textureResources.end())
                continue;

            vk::ImageLayout requiredLayout = vk::ImageLayout::eColorAttachmentOptimal;
            vk::ImageLayout currentLayout = m_textureLayouts[colorAttachment.handle.handle];

            auto &accessInfo = textureAccessInfo[colorAttachment.handle.handle];

            vk::AccessFlags dstAccess = vk::AccessFlagBits::eColorAttachmentWrite;
            if (colorAttachment.loadOp == vk::AttachmentLoadOp::eLoad)
            {
                dstAccess |= vk::AccessFlagBits::eColorAttachmentRead;
            }

            // 需要屏障（WAW 或 RAW）
            if (accessInfo.lastAccess != vk::AccessFlagBits::eNone || currentLayout != requiredLayout)
            {
                addImageBarrier(*compiledPass, colorAttachment.handle, currentLayout, requiredLayout,
                                accessInfo.lastAccess, dstAccess, accessInfo.lastStages,
                                vk::PipelineStageFlagBits::eColorAttachmentOutput);

                m_textureLayouts[colorAttachment.handle.handle] = requiredLayout;
            }

            // 更新访问信息
            accessInfo.lastStages = vk::PipelineStageFlagBits::eColorAttachmentOutput;
            accessInfo.lastAccess = dstAccess;
            accessInfo.wasWrite = true;
        }

        // 处理深度附件屏障（写入操作）
        if (pass->m_depthAttachment.handle.isValid())
        {
            auto it = m_textureResources.find(pass->m_depthAttachment.handle.handle);
            if (it != m_textureResources.end())
            {
                vk::ImageLayout requiredLayout = vk::ImageLayout::eDepthStencilAttachmentOptimal;
                vk::ImageLayout currentLayout = m_textureLayouts[pass->m_depthAttachment.handle.handle];

                auto &accessInfo = textureAccessInfo[pass->m_depthAttachment.handle.handle];

                vk::AccessFlags dstAccess = vk::AccessFlagBits::eDepthStencilAttachmentWrite;
                if (pass->m_depthAttachment.loadOp == vk::AttachmentLoadOp::eLoad)
                {
                    dstAccess |= vk::AccessFlagBits::eDepthStencilAttachmentRead;
                }

                // 需要屏障
                if (accessInfo.lastAccess != vk::AccessFlagBits::eNone || currentLayout != requiredLayout)
                {
                    addImageBarrier(*compiledPass, pass->m_depthAttachment.handle, currentLayout, requiredLayout,
                                    accessInfo.lastAccess, dstAccess, accessInfo.lastStages,
                                    vk::PipelineStageFlagBits::eEarlyFragmentTests |
                                        vk::PipelineStageFlagBits::eLateFragmentTests);

                    m_textureLayouts[pass->m_depthAttachment.handle.handle] = requiredLayout;
                }

                // 更新访问信息
                accessInfo.lastStages =
                    vk::PipelineStageFlagBits::eEarlyFragmentTests | vk::PipelineStageFlagBits::eLateFragmentTests;
                accessInfo.lastAccess = dstAccess;
                accessInfo.wasWrite = true;
            }
        }

        // 处理存储纹理写入
        for (const auto &textureWrite : pass->m_textureWrites)
        {
            auto it = m_textureResources.find(textureWrite.handle.handle);
            if (it == m_textureResources.end())
                continue;

            vk::ImageLayout requiredLayout = vk::ImageLayout::eGeneral;
            vk::ImageLayout currentLayout = m_textureLayouts[textureWrite.handle.handle];

            auto &accessInfo = textureAccessInfo[textureWrite.handle.handle];

            // 需要屏障
            if (accessInfo.lastAccess != vk::AccessFlagBits::eNone || currentLayout != requiredLayout)
            {
                addImageBarrier(*compiledPass, textureWrite.handle, currentLayout, requiredLayout,
                                accessInfo.lastAccess, textureWrite.access, accessInfo.lastStages, textureWrite.stages);

                m_textureLayouts[textureWrite.handle.handle] = requiredLayout;
            }

            // 更新访问信息
            accessInfo.lastStages = textureWrite.stages;
            accessInfo.lastAccess = textureWrite.access;
            accessInfo.wasWrite = true;
        }

        // 处理缓冲区读取
        for (const auto &bufferRead : pass->m_bufferReads)
        {
            auto it = m_bufferResources.find(bufferRead.handle.handle);
            if (it == m_bufferResources.end())
                continue;

            auto &accessInfo = bufferAccessInfo[bufferRead.handle.handle];

            // 如果上一次是写入操作，需要屏障
            if (accessInfo.wasWrite)
            {
                addBufferBarrier(*compiledPass, bufferRead.handle, accessInfo.lastAccess, bufferRead.access,
                                 accessInfo.lastStages, bufferRead.stages);
            }

            // 更新访问信息
            accessInfo.lastStages = bufferRead.stages;
            accessInfo.lastAccess = bufferRead.access;
            accessInfo.wasWrite = false;
        }

        // 处理缓冲区写入
        for (const auto &bufferWrite : pass->m_bufferWrites)
        {
            auto it = m_bufferResources.find(bufferWrite.handle.handle);
            if (it == m_bufferResources.end())
                continue;

            auto &accessInfo = bufferAccessInfo[bufferWrite.handle.handle];

            // 需要屏障（WAW 或 RAW）
            if (accessInfo.lastAccess != vk::AccessFlagBits::eNone)
            {
                addBufferBarrier(*compiledPass, bufferWrite.handle, accessInfo.lastAccess, bufferWrite.access,
                                 accessInfo.lastStages, bufferWrite.stages);
            }

            // 更新访问信息
            accessInfo.lastStages = bufferWrite.stages;
            accessInfo.lastAccess = bufferWrite.access;
            accessInfo.wasWrite = true;
        }
    }

    std::cout << "屏障计算完成" << std::endl;
}

// ==================== 验证辅助函数 ====================

void RenderGraph::validateResourceStates() const
{
    std::cout << "验证资源状态..." << std::endl;

    // 跟踪每个资源是否已被写入
    std::unordered_map<RDGResourceHandle, bool> textureWritten;
    std::unordered_map<RDGResourceHandle, bool> bufferWritten;

    // 遍历所有活跃Pass
    for (size_t passIndex = 0; passIndex < m_compiledPasses.size(); ++passIndex)
    {
        const auto &compiledPass = m_compiledPasses[passIndex];
        if (!compiledPass->isActive())
        {
            continue;
        }

        const RDGPass *pass = compiledPass->getOriginalPass();

        // 验证读取的纹理资源
        for (const auto &textureRead : pass->m_textureReads)
        {
            auto it = m_textureResources.find(textureRead.handle.handle);
            if (it == m_textureResources.end())
            {
                throw std::runtime_error("RenderGraph::validateResourceStates: Pass '" + pass->getName() +
                                         "' 尝试读取不存在的纹理资源");
            }

            const auto &resource = it->second;

            // 外部资源不需要验证写入状态
            if (resource->isExternal())
            {
                continue;
            }

            // 瞬态资源必须在读取前被写入
            if (textureWritten.find(textureRead.handle.handle) == textureWritten.end())
            {
                std::cerr << "警告: Pass '" << pass->getName() << "' 读取了未被写入的纹理资源 '" << resource->getName()
                          << "'" << std::endl;
            }
        }

        // 验证读取的缓冲区资源
        for (const auto &bufferRead : pass->m_bufferReads)
        {
            auto it = m_bufferResources.find(bufferRead.handle.handle);
            if (it == m_bufferResources.end())
            {
                throw std::runtime_error("RenderGraph::validateResourceStates: Pass '" + pass->getName() +
                                         "' 尝试读取不存在的缓冲区资源");
            }

            const auto &resource = it->second;

            // 外部资源不需要验证写入状态
            if (resource->isExternal())
            {
                continue;
            }

            // 瞬态资源必须在读取前被写入
            if (bufferWritten.find(bufferRead.handle.handle) == bufferWritten.end())
            {
                std::cerr << "警告: Pass '" << pass->getName() << "' 读取了未被写入的缓冲区资源 '"
                          << resource->getName() << "'" << std::endl;
            }
        }

        // 标记写入的纹理资源
        for (const auto &colorAttachment : pass->m_colorAttachments)
        {
            textureWritten[colorAttachment.handle.handle] = true;
        }

        if (pass->m_depthAttachment.handle.isValid())
        {
            textureWritten[pass->m_depthAttachment.handle.handle] = true;
        }

        for (const auto &textureWrite : pass->m_textureWrites)
        {
            textureWritten[textureWrite.handle.handle] = true;
        }

        // 标记写入的缓冲区资源
        for (const auto &bufferWrite : pass->m_bufferWrites)
        {
            bufferWritten[bufferWrite.handle.handle] = true;
        }
    }

    std::cout << "资源状态验证完成" << std::endl;
}

// ==================== 资源分配辅助函数 ====================

vkcore::Image *RenderGraph::allocateTransientTexture(RDGTextureResource &resource)
{
    const auto &desc = resource.getDesc();

    // 尝试从池中复用资源（现在会自动检查规格匹配）
    vkcore::Image *reusedImage = m_texturePool.tryAcquire(desc, resource.getLifetime());

    if (reusedImage)
    {
        std::cout << "  复用纹理资源: " << desc.name << " (格式: " << vk::to_string(desc.format)
                  << ", 尺寸: " << desc.extent.width << "x" << desc.extent.height << ")" << std::endl;
        return reusedImage;
    }

    // 创建ImageDesc
    vkcore::ImageDesc imageDesc{};
    imageDesc.format = desc.format;
    imageDesc.extent = desc.extent;
    imageDesc.usage = desc.usage;
    imageDesc.mipLevels = desc.mipLevels;
    imageDesc.arrayLayers = desc.arrayLayers;
    imageDesc.samples = desc.samples;
    imageDesc.tiling = desc.tiling;

    // 创建新的Image对象
    auto image = std::make_unique<vkcore::Image>(desc.name, m_device, m_allocator, imageDesc);
    std::cout << "  创建新纹理资源: " << desc.name << " (格式: " << vk::to_string(desc.format)
              << ", 尺寸: " << desc.extent.width << "x" << desc.extent.height << ")" << std::endl;

    vkcore::Image *imagePtr = image.get();
    m_frameTextures.push_back(std::move(image));

    return imagePtr;
}

vkcore::Buffer *RenderGraph::allocateTransientBuffer(RDGBufferResource &resource)
{
    const auto &desc = resource.getDesc();

    // 尝试从池中复用资源（现在会自动检查规格匹配）
    vkcore::Buffer *reusedBuffer = m_bufferPool.tryAcquire(desc, resource.getLifetime());

    if (reusedBuffer)
    {
        std::cout << "  复用缓冲区资源: " << desc.name << " (大小: " << desc.size << " 字节)" << std::endl;
        return reusedBuffer;
    }

    // 创建BufferDesc
    vkcore::BufferDesc bufferDesc{};
    bufferDesc.size = desc.size;
    bufferDesc.usageFlags = desc.usage;

    // 创建新的Buffer对象
    auto buffer = std::make_unique<vkcore::Buffer>(desc.name, m_device, m_allocator, bufferDesc);
    std::cout << "  创建新缓冲区资源: " << desc.name << " (大小: " << desc.size << " 字节)" << std::endl;

    vkcore::Buffer *bufferPtr = buffer.get();
    m_frameBuffers.push_back(std::move(buffer));

    return bufferPtr;
}

// ==================== 屏障计算辅助函数 ====================

vk::ImageLayout RenderGraph::computeImageLayout(RDGTextureHandle handle, const RDGPass::TextureAccess &access) const
{
    // 禁用未使用参数警告 - handle保留用于未来扩展
    (void)handle;

    // 根据访问类型推断图像布局
    if (access.access & vk::AccessFlagBits::eColorAttachmentWrite)
    {
        return vk::ImageLayout::eColorAttachmentOptimal;
    }
    else if (access.access & vk::AccessFlagBits::eDepthStencilAttachmentWrite)
    {
        return vk::ImageLayout::eDepthStencilAttachmentOptimal;
    }
    else if (access.access & vk::AccessFlagBits::eShaderRead)
    {
        return vk::ImageLayout::eShaderReadOnlyOptimal;
    }
    else if (access.access & vk::AccessFlagBits::eShaderWrite)
    {
        return vk::ImageLayout::eGeneral;
    }
    else if (access.access & vk::AccessFlagBits::eTransferRead)
    {
        return vk::ImageLayout::eTransferSrcOptimal;
    }
    else if (access.access & vk::AccessFlagBits::eTransferWrite)
    {
        return vk::ImageLayout::eTransferDstOptimal;
    }

    return vk::ImageLayout::eGeneral;
}

void RenderGraph::addImageBarrier(RDGCompiledPass &pass, RDGTextureHandle handle, vk::ImageLayout oldLayout,
                                  vk::ImageLayout newLayout, vk::AccessFlags srcAccess, vk::AccessFlags dstAccess,
                                  vk::PipelineStageFlags srcStages, vk::PipelineStageFlags dstStages)
{
    RDGBarrier barrier{};
    barrier.type = RDGBarrier::Image;
    barrier.handle = handle.handle;
    barrier.srcStages = srcStages;
    barrier.dstStages = dstStages;
    barrier.srcAccess = srcAccess;
    barrier.dstAccess = dstAccess;
    barrier.oldLayout = oldLayout;
    barrier.newLayout = newLayout;

    // 设置子资源范围
    barrier.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = VK_REMAINING_MIP_LEVELS;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = VK_REMAINING_ARRAY_LAYERS;

    // 处理深度格式
    auto it = m_textureResources.find(handle.handle);
    if (it != m_textureResources.end())
    {
        vk::Format format = it->second->getDesc().format;
        if (format == vk::Format::eD16Unorm || format == vk::Format::eD32Sfloat ||
            format == vk::Format::eD16UnormS8Uint || format == vk::Format::eD24UnormS8Uint ||
            format == vk::Format::eD32SfloatS8Uint)
        {
            barrier.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eDepth;
            if (format == vk::Format::eD16UnormS8Uint || format == vk::Format::eD24UnormS8Uint ||
                format == vk::Format::eD32SfloatS8Uint)
            {
                barrier.subresourceRange.aspectMask |= vk::ImageAspectFlagBits::eStencil;
            }
        }
    }

    pass.addBarrier(barrier);
}

void RenderGraph::addBufferBarrier(RDGCompiledPass &pass, RDGBufferHandle handle, vk::AccessFlags srcAccess,
                                   vk::AccessFlags dstAccess, vk::PipelineStageFlags srcStages,
                                   vk::PipelineStageFlags dstStages)
{
    RDGBarrier barrier{};
    barrier.type = RDGBarrier::Buffer;
    barrier.handle = handle.handle;
    barrier.srcStages = srcStages;
    barrier.dstStages = dstStages;
    barrier.srcAccess = srcAccess;
    barrier.dstAccess = dstAccess;

    pass.addBarrier(barrier);
}

// ==================== 执行辅助函数 ====================

void RenderGraph::executeBarriers(vk::CommandBuffer cmd, const std::vector<RDGBarrier> &barriers)
{
    if (barriers.empty())
    {
        return;
    }

    std::vector<vk::ImageMemoryBarrier> imageBarriers;
    std::vector<vk::BufferMemoryBarrier> bufferBarriers;

    vk::PipelineStageFlags srcStages = vk::PipelineStageFlagBits::eNone;
    vk::PipelineStageFlags dstStages = vk::PipelineStageFlagBits::eNone;

    // 收集所有屏障
    for (const auto &barrier : barriers)
    {
        srcStages |= barrier.srcStages;
        dstStages |= barrier.dstStages;

        if (barrier.type == RDGBarrier::Image)
        {
            auto it = m_textureResources.find(barrier.handle);
            if (it != m_textureResources.end())
            {
                vkcore::Image *image = it->second->getPhysicalImage();
                if (image)
                {
                    vk::ImageMemoryBarrier imageBarrier{};
                    imageBarrier.srcAccessMask = barrier.srcAccess;
                    imageBarrier.dstAccessMask = barrier.dstAccess;
                    imageBarrier.oldLayout = barrier.oldLayout;
                    imageBarrier.newLayout = barrier.newLayout;
                    imageBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                    imageBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                    imageBarrier.image = image->get();
                    imageBarrier.subresourceRange = barrier.subresourceRange;

                    imageBarriers.push_back(imageBarrier);
                }
            }
        }
        else if (barrier.type == RDGBarrier::Buffer)
        {
            auto it = m_bufferResources.find(barrier.handle);
            if (it != m_bufferResources.end())
            {
                vkcore::Buffer *buffer = it->second->getPhysicalBuffer();
                if (buffer)
                {
                    vk::BufferMemoryBarrier bufferBarrier{};
                    bufferBarrier.srcAccessMask = barrier.srcAccess;
                    bufferBarrier.dstAccessMask = barrier.dstAccess;
                    bufferBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                    bufferBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                    bufferBarrier.buffer = buffer->get();
                    bufferBarrier.offset = 0;
                    bufferBarrier.size = VK_WHOLE_SIZE;

                    bufferBarriers.push_back(bufferBarrier);
                }
            }
        }
    }

    // 执行屏障
    if (!imageBarriers.empty() || !bufferBarriers.empty())
    {
        cmd.pipelineBarrier(srcStages, dstStages, vk::DependencyFlags{}, nullptr, bufferBarriers, imageBarriers);
    }
}

void RenderGraph::beginGraphicsPass(vk::CommandBuffer cmd, const RDGPass &pass)
{
    if (m_insideGraphicsPass)
    {
        throw std::runtime_error("RenderGraph::beginGraphicsPass: Already inside graphics pass");
    }

    // 收集颜色附件
    std::vector<vk::RenderingAttachmentInfo> colorAttachments;
    colorAttachments.reserve(pass.m_colorAttachments.size());

    for (const auto &colorAttachment : pass.m_colorAttachments)
    {
        auto it = m_textureResources.find(colorAttachment.handle.handle);
        if (it != m_textureResources.end())
        {
            const auto &resource = it->second;

            // 检查是否是SwapChain图像
            if (resource->isSwapChainImage())
            {
                auto swapChainIt = m_swapChainMapping.find(colorAttachment.handle.handle);
                if (swapChainIt != m_swapChainMapping.end())
                {
                    vkcore::SwapChain *swapChain = swapChainIt->second;
                    uint32_t imageIndex = resource->getSwapChainImageIndex();

                    vk::RenderingAttachmentInfo attachmentInfo{};
                    attachmentInfo.imageView = swapChain->getImageView(imageIndex);
                    attachmentInfo.imageLayout = vk::ImageLayout::eColorAttachmentOptimal;
                    attachmentInfo.loadOp = colorAttachment.loadOp;
                    attachmentInfo.storeOp = colorAttachment.storeOp;
                    attachmentInfo.clearValue.color = colorAttachment.clearValue;

                    colorAttachments.push_back(attachmentInfo);
                }
                else
                {
                    std::cerr << "警告: SwapChain图像缺少映射信息" << std::endl;
                }
                continue;
            }

            // 普通纹理资源
            vkcore::Image *image = resource->getPhysicalImage();
            if (image)
            {
                vk::RenderingAttachmentInfo attachmentInfo{};
                attachmentInfo.imageView = image->getView();
                attachmentInfo.imageLayout = vk::ImageLayout::eColorAttachmentOptimal;
                attachmentInfo.loadOp = colorAttachment.loadOp;
                attachmentInfo.storeOp = colorAttachment.storeOp;
                attachmentInfo.clearValue.color = colorAttachment.clearValue;

                colorAttachments.push_back(attachmentInfo);
            }
        }
    }

    // 处理深度附件
    vk::RenderingAttachmentInfo depthAttachment{};
    bool hasDepthAttachment = false;

    if (pass.m_depthAttachment.handle.isValid())
    {
        auto it = m_textureResources.find(pass.m_depthAttachment.handle.handle);
        if (it != m_textureResources.end())
        {
            vkcore::Image *image = it->second->getPhysicalImage();
            if (image)
            {
                depthAttachment.imageView = image->getView();
                depthAttachment.imageLayout = vk::ImageLayout::eDepthStencilAttachmentOptimal;
                depthAttachment.loadOp = pass.m_depthAttachment.loadOp;
                depthAttachment.storeOp = pass.m_depthAttachment.storeOp;
                depthAttachment.clearValue.depthStencil = pass.m_depthAttachment.clearValue;
                hasDepthAttachment = true;
            }
        }
    }

    // 计算渲染区域（使用第一个颜色附件的尺寸，或深度附件的尺寸）
    vk::Extent2D renderArea{0, 0};

    if (!pass.m_colorAttachments.empty())
    {
        auto it = m_textureResources.find(pass.m_colorAttachments[0].handle.handle);
        if (it != m_textureResources.end())
        {
            const auto &resource = it->second;

            // 如果是SwapChain图像，从SwapChain获取尺寸
            if (resource->isSwapChainImage())
            {
                auto swapChainIt = m_swapChainMapping.find(pass.m_colorAttachments[0].handle.handle);
                if (swapChainIt != m_swapChainMapping.end())
                {
                    renderArea = swapChainIt->second->getSwapchainExtent();
                }
            }
            else
            {
                const auto &extent = resource->getDesc().extent;
                renderArea = vk::Extent2D{extent.width, extent.height};
            }
        }
    }
    else if (hasDepthAttachment)
    {
        auto it = m_textureResources.find(pass.m_depthAttachment.handle.handle);
        if (it != m_textureResources.end())
        {
            const auto &extent = it->second->getDesc().extent;
            renderArea = vk::Extent2D{extent.width, extent.height};
        }
    }

    // 开始动态渲染
    if (renderArea.width > 0 && renderArea.height > 0)
    {
        vk::RenderingInfo renderingInfo{};
        renderingInfo.renderArea = vk::Rect2D{{0, 0}, renderArea};
        renderingInfo.layerCount = 1;
        renderingInfo.colorAttachmentCount = static_cast<uint32_t>(colorAttachments.size());
        renderingInfo.pColorAttachments = colorAttachments.data();

        if (hasDepthAttachment)
        {
            renderingInfo.pDepthAttachment = &depthAttachment;
        }

        cmd.beginRendering(renderingInfo);
        m_insideGraphicsPass = true;
    }
}

void RenderGraph::endGraphicsPass(vk::CommandBuffer cmd)
{
    if (m_insideGraphicsPass)
    {
        cmd.endRendering();
        m_insideGraphicsPass = false;
    }
}

// ==================== 资源访问接口实现 ====================

vkcore::Image *RenderGraph::getPhysicalTexture(RDGTextureHandle handle) const
{
    if (!handle.isValid())
    {
        return nullptr;
    }

    auto it = m_textureResources.find(handle.handle);
    if (it == m_textureResources.end())
    {
        return nullptr;
    }

    return it->second->getPhysicalImage();
}

vkcore::Buffer *RenderGraph::getPhysicalBuffer(RDGBufferHandle handle) const
{
    if (!handle.isValid())
    {
        return nullptr;
    }

    auto it = m_bufferResources.find(handle.handle);
    if (it == m_bufferResources.end())
    {
        return nullptr;
    }

    return it->second->getPhysicalBuffer();
}

vk::ImageLayout RenderGraph::getTextureLayout(RDGTextureHandle handle) const
{
    if (!handle.isValid())
    {
        return vk::ImageLayout::eUndefined;
    }

    auto it = m_textureLayouts.find(handle.handle);
    if (it == m_textureLayouts.end())
    {
        return vk::ImageLayout::eUndefined;
    }

    return it->second;
}

vk::Sampler RenderGraph::getSampler(RDGSamplerType type) const
{
    // 延迟创建采样器
    if (!m_samplersCreated)
    {
        const_cast<RenderGraph *>(this)->createSamplers();
    }

    size_t index = static_cast<size_t>(type);
    if (index >= m_samplers.size())
    {
        return vk::Sampler{};
    }

    return m_samplers[index];
}

// ==================== 采样器管理实现 ====================

void RenderGraph::createSamplers()
{
    if (m_samplersCreated)
    {
        return;
    }

    vk::Device device = m_device.get();

    // 基础采样器信息
    vk::SamplerCreateInfo samplerInfo{};
    samplerInfo.magFilter = vk::Filter::eLinear;
    samplerInfo.minFilter = vk::Filter::eLinear;
    samplerInfo.mipmapMode = vk::SamplerMipmapMode::eLinear;
    samplerInfo.addressModeU = vk::SamplerAddressMode::eClampToEdge;
    samplerInfo.addressModeV = vk::SamplerAddressMode::eClampToEdge;
    samplerInfo.addressModeW = vk::SamplerAddressMode::eClampToEdge;
    samplerInfo.mipLodBias = 0.0f;
    samplerInfo.anisotropyEnable = VK_FALSE;
    samplerInfo.maxAnisotropy = 1.0f;
    samplerInfo.compareEnable = VK_FALSE;
    samplerInfo.compareOp = vk::CompareOp::eAlways;
    samplerInfo.minLod = 0.0f;
    samplerInfo.maxLod = VK_LOD_CLAMP_NONE;
    samplerInfo.borderColor = vk::BorderColor::eFloatOpaqueBlack;
    samplerInfo.unnormalizedCoordinates = VK_FALSE;

    // 创建各种采样器
    try
    {
        // NearestClamp
        samplerInfo.magFilter = vk::Filter::eNearest;
        samplerInfo.minFilter = vk::Filter::eNearest;
        samplerInfo.addressModeU = vk::SamplerAddressMode::eClampToEdge;
        samplerInfo.addressModeV = vk::SamplerAddressMode::eClampToEdge;
        samplerInfo.addressModeW = vk::SamplerAddressMode::eClampToEdge;
        m_samplers[static_cast<size_t>(RDGSamplerType::NearestClamp)] = device.createSampler(samplerInfo);

        // NearestRepeat
        samplerInfo.addressModeU = vk::SamplerAddressMode::eRepeat;
        samplerInfo.addressModeV = vk::SamplerAddressMode::eRepeat;
        samplerInfo.addressModeW = vk::SamplerAddressMode::eRepeat;
        m_samplers[static_cast<size_t>(RDGSamplerType::NearestRepeat)] = device.createSampler(samplerInfo);

        // LinearClamp
        samplerInfo.magFilter = vk::Filter::eLinear;
        samplerInfo.minFilter = vk::Filter::eLinear;
        samplerInfo.addressModeU = vk::SamplerAddressMode::eClampToEdge;
        samplerInfo.addressModeV = vk::SamplerAddressMode::eClampToEdge;
        samplerInfo.addressModeW = vk::SamplerAddressMode::eClampToEdge;
        m_samplers[static_cast<size_t>(RDGSamplerType::LinearClamp)] = device.createSampler(samplerInfo);

        // LinearRepeat
        samplerInfo.addressModeU = vk::SamplerAddressMode::eRepeat;
        samplerInfo.addressModeV = vk::SamplerAddressMode::eRepeat;
        samplerInfo.addressModeW = vk::SamplerAddressMode::eRepeat;
        m_samplers[static_cast<size_t>(RDGSamplerType::LinearRepeat)] = device.createSampler(samplerInfo);

        // AnisotropicClamp
        samplerInfo.anisotropyEnable = VK_TRUE;
        samplerInfo.maxAnisotropy = m_device.getPhysicalDevice().getProperties().limits.maxSamplerAnisotropy;
        samplerInfo.addressModeU = vk::SamplerAddressMode::eClampToEdge;
        samplerInfo.addressModeV = vk::SamplerAddressMode::eClampToEdge;
        samplerInfo.addressModeW = vk::SamplerAddressMode::eClampToEdge;
        m_samplers[static_cast<size_t>(RDGSamplerType::AnisotropicClamp)] = device.createSampler(samplerInfo);

        // AnisotropicRepeat
        samplerInfo.addressModeU = vk::SamplerAddressMode::eRepeat;
        samplerInfo.addressModeV = vk::SamplerAddressMode::eRepeat;
        samplerInfo.addressModeW = vk::SamplerAddressMode::eRepeat;
        m_samplers[static_cast<size_t>(RDGSamplerType::AnisotropicRepeat)] = device.createSampler(samplerInfo);

        // ShadowPCF
        samplerInfo.anisotropyEnable = VK_FALSE;
        samplerInfo.maxAnisotropy = 1.0f;
        samplerInfo.compareEnable = VK_TRUE;
        samplerInfo.compareOp = vk::CompareOp::eLessOrEqual;
        samplerInfo.addressModeU = vk::SamplerAddressMode::eClampToBorder;
        samplerInfo.addressModeV = vk::SamplerAddressMode::eClampToBorder;
        samplerInfo.addressModeW = vk::SamplerAddressMode::eClampToBorder;
        samplerInfo.borderColor = vk::BorderColor::eFloatOpaqueWhite;
        m_samplers[static_cast<size_t>(RDGSamplerType::ShadowPCF)] = device.createSampler(samplerInfo);

        m_samplersCreated = true;
    }
    catch (const vk::SystemError &e)
    {
        std::cerr << "Failed to create samplers: " << e.what() << std::endl;
        destroySamplers();
        throw;
    }
}

void RenderGraph::destroySamplers()
{
    if (!m_samplersCreated)
    {
        return;
    }

    vk::Device device = m_device.get();

    for (auto sampler : m_samplers)
    {
        if (sampler)
        {
            device.destroySampler(sampler);
        }
    }

    m_samplers.fill(vk::Sampler{});
    m_samplersCreated = false;
}

} // namespace rendercore