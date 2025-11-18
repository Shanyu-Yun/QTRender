/**
 * @file RDGBuilder.cpp
 * @brief RDGBuilder类的实现
 */

#include "../public/RDGBuilder.hpp"
#include "../../VulkanCore/public/SwapChain.hpp"
#include "RenderGraph.hpp"
#include <stdexcept>

namespace rendercore
{

// ==================== 构造函数和析构函数 ====================

RDGBuilder::RDGBuilder(vkcore::Device &device, vkcore::CommandPoolManager &cmdManager, VmaAllocator allocator)
    : m_pimpl(std::make_unique<RenderGraph>(device, cmdManager, allocator)), m_executed(false)
{
}

RDGBuilder::~RDGBuilder()
{
    // 如果未显式调用execute()，析构函数中自动执行
    if (!m_executed)
    {
        try
        {
            execute(nullptr); // 使用默认同步（无同步信息）
        }
        catch (const std::exception &)
        {
            // 在析构函数中不应抛出异常，只记录错误
            // 实际项目中应使用日志系统
        }
    }
}

// ==================== Pass管理 ====================

RDGPass &RDGBuilder::addPass(std::string name, RDGPass::ExecuteCallback &&callback)
{
    validateState();
    return m_pimpl->addPass(std::move(name), std::move(callback));
}

RDGPass &RDGBuilder::addPass(std::string name, RDGPass::ExecuteCallbackEx &&callback)
{
    validateState();
    return m_pimpl->addPassEx(std::move(name), std::move(callback));
}

// ==================== 瞬态资源创建 ====================

RDGTextureHandle RDGBuilder::createTexture(const RDGTextureDesc &desc)
{
    validateState();

    if (!desc.isValid())
    {
        throw std::invalid_argument("RDGBuilder::createTexture: Invalid texture description");
    }

    return m_pimpl->createTransientTexture(desc);
}

RDGBufferHandle RDGBuilder::createBuffer(const RDGBufferDesc &desc)
{
    validateState();

    if (!desc.isValid())
    {
        throw std::invalid_argument("RDGBuilder::createBuffer: Invalid buffer description");
    }

    return m_pimpl->createTransientBuffer(desc);
}

// ==================== 外部资源导入 ====================

RDGTextureHandle RDGBuilder::registerExternalTexture(vkcore::Image *image, const std::string &name,
                                                     vk::ImageLayout currentLayout)
{
    validateState();

    if (!image)
    {
        throw std::invalid_argument("RDGBuilder::registerExternalTexture: Image cannot be null");
    }

    return m_pimpl->registerExternalTexture(image, name, currentLayout);
}

RDGBufferHandle RDGBuilder::registerExternalBuffer(vkcore::Buffer *buffer, const std::string &name)
{
    validateState();

    if (!buffer)
    {
        throw std::invalid_argument("RDGBuilder::registerExternalBuffer: Buffer cannot be null");
    }

    return m_pimpl->registerExternalBuffer(buffer, name);
}

// ==================== 便利函数 ====================

RDGTextureHandle RDGBuilder::getSwapChainAttachment(vkcore::SwapChain &swapChain, uint32_t imageIndex)
{
    validateState();
    return m_pimpl->importSwapChainImage(swapChain, imageIndex);
}

RDGTextureHandle RDGBuilder::createTexture2D(const std::string &name, vk::Format format, uint32_t width,
                                             uint32_t height, vk::ImageUsageFlags usage)
{
    validateState();

    RDGTextureDesc desc(name, format, width, height, usage);
    return createTexture(desc);
}

RDGTextureHandle RDGBuilder::createDepthBuffer(const std::string &name, uint32_t width, uint32_t height,
                                               vk::Format format)
{
    validateState();

    vk::ImageUsageFlags usage = vk::ImageUsageFlagBits::eDepthStencilAttachment;

    RDGTextureDesc desc(name, format, width, height, usage);
    return createTexture(desc);
}

RDGTextureHandle RDGBuilder::createColorBuffer(const std::string &name, uint32_t width, uint32_t height,
                                               vk::Format format)
{
    validateState();

    vk::ImageUsageFlags usage = vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eSampled;

    RDGTextureDesc desc(name, format, width, height, usage);
    return createTexture(desc);
}

// ==================== 执行控制 ====================

void RDGBuilder::execute(RDGSyncInfo *syncInfo)
{
    if (m_executed)
    {
        throw std::runtime_error("RDGBuilder::execute: Already executed");
    }

    // 编译渲染图
    m_pimpl->compile();

    // 执行渲染图（传递同步信息）
    m_pimpl->execute(syncInfo);

    m_executed = true;
}

// ==================== 查询接口 ====================

size_t RDGBuilder::getPassCount() const
{
    return m_pimpl->getPassCount();
}

size_t RDGBuilder::getTransientResourceCount() const
{
    return m_pimpl->getTransientResourceCount();
}

void RDGBuilder::setDebugName(const std::string &name)
{
    m_pimpl->setDebugName(name);
}

// ==================== 私有方法 ====================

void RDGBuilder::validateState() const
{
    if (m_executed)
    {
        throw std::runtime_error("RDGBuilder: Cannot modify builder after execution");
    }
}

} // namespace rendercore