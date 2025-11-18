/**
 * @file RDGPass.cpp
 * @brief RDGPass类的实现
 */

#include "../public/RDGPass.hpp"
#include <stdexcept>

namespace rendercore
{

// ==================== 构造函数 ====================

RDGPass::RDGPass(std::string name, ExecuteCallback &&callback)
    : m_name(std::move(name)), m_executeCallback(std::move(callback)),
      m_useExtendedCallback(false), m_depthAttachment{kInvalidTextureHandle} // 初始化为无效句柄
{
    if (!m_executeCallback)
    {
        throw std::invalid_argument("RDGPass: ExecuteCallback cannot be null");
    }
}

RDGPass::RDGPass(std::string name, ExecuteCallbackEx &&callback)
    : m_name(std::move(name)), m_executeCallbackEx(std::move(callback)),
      m_useExtendedCallback(true), m_depthAttachment{kInvalidTextureHandle} // 初始化为无效句柄
{
    if (!m_executeCallbackEx)
    {
        throw std::invalid_argument("RDGPass: ExecuteCallbackEx cannot be null");
    }
}

// ==================== 资源读依赖 ====================

RDGPass &RDGPass::readTexture(RDGTextureHandle handle, vk::PipelineStageFlags stages, vk::AccessFlags access)
{
    if (!handle.isValid())
    {
        throw std::invalid_argument("RDGPass::readTexture: Invalid texture handle");
    }

    TextureAccess textureAccess{};
    textureAccess.handle = handle;
    textureAccess.stages = stages;
    textureAccess.access = access;

    // 推断图像布局
    if (access & vk::AccessFlagBits::eShaderRead)
    {
        textureAccess.layout = vk::ImageLayout::eShaderReadOnlyOptimal;
    }
    else if (access & vk::AccessFlagBits::eInputAttachmentRead)
    {
        textureAccess.layout = vk::ImageLayout::eShaderReadOnlyOptimal;
    }
    else
    {
        textureAccess.layout = vk::ImageLayout::eGeneral;
    }

    m_textureReads.push_back(textureAccess);
    return *this;
}

RDGPass &RDGPass::readBuffer(RDGBufferHandle handle, vk::PipelineStageFlags stages, vk::AccessFlags access)
{
    if (!handle.isValid())
    {
        throw std::invalid_argument("RDGPass::readBuffer: Invalid buffer handle");
    }

    BufferAccess bufferAccess{};
    bufferAccess.handle = handle;
    bufferAccess.stages = stages;
    bufferAccess.access = access;

    m_bufferReads.push_back(bufferAccess);
    return *this;
}

// ==================== 渲染目标依赖 ====================

RDGPass &RDGPass::writeColorAttachment(RDGTextureHandle handle, vk::AttachmentLoadOp loadOp,
                                       vk::AttachmentStoreOp storeOp, vk::ClearColorValue clearValue)
{
    if (!handle.isValid())
    {
        throw std::invalid_argument("RDGPass::writeColorAttachment: Invalid texture handle");
    }

    ColorAttachment colorAttachment{};
    colorAttachment.handle = handle;
    colorAttachment.loadOp = loadOp;
    colorAttachment.storeOp = storeOp;
    colorAttachment.clearValue = clearValue;

    m_colorAttachments.push_back(colorAttachment);
    return *this;
}

RDGPass &RDGPass::writeDepthStencilAttachment(RDGTextureHandle handle, vk::AttachmentLoadOp loadOp,
                                              vk::AttachmentStoreOp storeOp, vk::AttachmentLoadOp stencilLoadOp,
                                              vk::AttachmentStoreOp stencilStoreOp,
                                              vk::ClearDepthStencilValue clearValue)
{
    if (!handle.isValid())
    {
        throw std::invalid_argument("RDGPass::writeDepthStencilAttachment: Invalid texture handle");
    }

    if (m_depthAttachment.handle.isValid())
    {
        throw std::runtime_error("RDGPass::writeDepthStencilAttachment: Depth attachment already set");
    }

    m_depthAttachment.handle = handle;
    m_depthAttachment.loadOp = loadOp;
    m_depthAttachment.storeOp = storeOp;
    m_depthAttachment.stencilLoadOp = stencilLoadOp;
    m_depthAttachment.stencilStoreOp = stencilStoreOp;
    m_depthAttachment.clearValue = clearValue;

    return *this;
}

// ==================== 计算/存储资源依赖 ====================

RDGPass &RDGPass::writeStorageTexture(RDGTextureHandle handle, vk::PipelineStageFlags stages, vk::AccessFlags access)
{
    if (!handle.isValid())
    {
        throw std::invalid_argument("RDGPass::writeStorageTexture: Invalid texture handle");
    }

    TextureAccess textureAccess{};
    textureAccess.handle = handle;
    textureAccess.stages = stages;
    textureAccess.access = access;
    textureAccess.layout = vk::ImageLayout::eGeneral; // 存储图像通常使用General布局

    m_textureWrites.push_back(textureAccess);
    return *this;
}

RDGPass &RDGPass::writeStorageBuffer(RDGBufferHandle handle, vk::PipelineStageFlags stages, vk::AccessFlags access)
{
    if (!handle.isValid())
    {
        throw std::invalid_argument("RDGPass::writeStorageBuffer: Invalid buffer handle");
    }

    BufferAccess bufferAccess{};
    bufferAccess.handle = handle;
    bufferAccess.stages = stages;
    bufferAccess.access = access;

    m_bufferWrites.push_back(bufferAccess);
    return *this;
}

} // namespace rendercore
