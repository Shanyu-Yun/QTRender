/**
 * @file RDGResourceAccessor.cpp
 * @brief RDGResourceAccessor 实现
 */

#include "../public/RDGResourceAccessor.hpp"
#include "../../VulkanCore/public/VKResource.hpp"
#include "../private/RenderGraph.hpp"


namespace rendercore
{

RDGResourceAccessor::RDGResourceAccessor(RenderGraph *renderGraph) : m_renderGraph(renderGraph)
{
}

// ==================== 纹理资源访问 ====================

vk::ImageView RDGResourceAccessor::getTextureView(RDGTextureHandle handle) const
{
    vkcore::Image *image = getTexture(handle);
    return image ? image->getView() : vk::ImageView{};
}

vkcore::Image *RDGResourceAccessor::getTexture(RDGTextureHandle handle) const
{
    if (!m_renderGraph || !handle.isValid())
    {
        return nullptr;
    }

    return m_renderGraph->getPhysicalTexture(handle);
}

vk::ImageLayout RDGResourceAccessor::getTextureLayout(RDGTextureHandle handle) const
{
    if (!m_renderGraph || !handle.isValid())
    {
        return vk::ImageLayout::eUndefined;
    }

    return m_renderGraph->getTextureLayout(handle);
}

// ==================== 缓冲区资源访问 ====================

vk::Buffer RDGResourceAccessor::getBuffer(RDGBufferHandle handle) const
{
    vkcore::Buffer *buffer = getBufferObject(handle);
    return buffer ? buffer->get() : vk::Buffer{};
}

vkcore::Buffer *RDGResourceAccessor::getBufferObject(RDGBufferHandle handle) const
{
    if (!m_renderGraph || !handle.isValid())
    {
        return nullptr;
    }

    return m_renderGraph->getPhysicalBuffer(handle);
}

vk::DeviceAddress RDGResourceAccessor::getBufferDeviceAddress(RDGBufferHandle handle) const
{
    vkcore::Buffer *buffer = getBufferObject(handle);
    if (!buffer)
    {
        return 0;
    }

    // 注意：需要在 vkcore::Buffer 中添加 getDeviceAddress() 方法
    // 暂时返回 0，未来可以实现
    return 0;
}

// ==================== 采样器访问 ====================

vk::Sampler RDGResourceAccessor::getSampler(RDGSamplerType type) const
{
    if (!m_renderGraph)
    {
        return vk::Sampler{};
    }

    return m_renderGraph->getSampler(type);
}

} // namespace rendercore
