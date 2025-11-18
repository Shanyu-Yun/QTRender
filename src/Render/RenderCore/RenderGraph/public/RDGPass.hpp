/**
 * @file RDGPass.hpp
 * @brief RenderGraph Pass 定义
 */

#pragma once

#include "RDGHandle.hpp"
#include <functional>
#include <string>
#include <vector>
#include <vulkan/vulkan.hpp>

namespace rendercore
{

/**
 * @class RDGPass
 * @brief 渲染图中的一个Pass
 */
class RDGPass
{
  public:
    // 回调函数类型
    using ExecuteCallback = std::function<void(vk::CommandBuffer)>;
    using ExecuteCallbackEx = std::function<void(vk::CommandBuffer, const class RDGResourceAccessor &)>;

    // 纹理访问信息
    struct TextureAccess
    {
        RDGTextureHandle handle;
        vk::PipelineStageFlags stages;
        vk::AccessFlags access;
        vk::ImageLayout layout;
    };

    // 缓冲区访问信息
    struct BufferAccess
    {
        RDGBufferHandle handle;
        vk::PipelineStageFlags stages;
        vk::AccessFlags access;
    };

    // 颜色附件信息
    struct ColorAttachment
    {
        RDGTextureHandle handle;
        vk::AttachmentLoadOp loadOp;
        vk::AttachmentStoreOp storeOp;
        vk::ClearColorValue clearValue;
    };

    // 深度附件信息
    struct DepthAttachment
    {
        RDGTextureHandle handle;
        vk::AttachmentLoadOp loadOp;
        vk::AttachmentStoreOp storeOp;
        vk::AttachmentLoadOp stencilLoadOp;
        vk::AttachmentStoreOp stencilStoreOp;
        vk::ClearDepthStencilValue clearValue;
    };

  public:
    RDGPass(std::string name, ExecuteCallback &&callback);
    RDGPass(std::string name, ExecuteCallbackEx &&callback);

    // 资源读依赖
    RDGPass &readTexture(RDGTextureHandle handle, vk::PipelineStageFlags stages, vk::AccessFlags access);
    RDGPass &readBuffer(RDGBufferHandle handle, vk::PipelineStageFlags stages, vk::AccessFlags access);

    // 渲染目标依赖
    RDGPass &writeColorAttachment(RDGTextureHandle handle, vk::AttachmentLoadOp loadOp = vk::AttachmentLoadOp::eClear,
                                  vk::AttachmentStoreOp storeOp = vk::AttachmentStoreOp::eStore,
                                  vk::ClearColorValue clearValue = {});

    RDGPass &writeDepthAttachment(RDGTextureHandle handle, vk::AttachmentLoadOp loadOp = vk::AttachmentLoadOp::eClear,
                                  vk::AttachmentStoreOp storeOp = vk::AttachmentStoreOp::eStore,
                                  vk::ClearDepthStencilValue clearValue = {1.0f, 0});

    RDGPass &writeDepthStencilAttachment(RDGTextureHandle handle,
                                         vk::AttachmentLoadOp loadOp = vk::AttachmentLoadOp::eClear,
                                         vk::AttachmentStoreOp storeOp = vk::AttachmentStoreOp::eStore,
                                         vk::AttachmentLoadOp stencilLoadOp = vk::AttachmentLoadOp::eClear,
                                         vk::AttachmentStoreOp stencilStoreOp = vk::AttachmentStoreOp::eStore,
                                         vk::ClearDepthStencilValue clearValue = {1.0f, 0});

    // 资源写依赖（计算/存储）
    RDGPass &writeStorageTexture(RDGTextureHandle handle, vk::PipelineStageFlags stages, vk::AccessFlags access);
    RDGPass &writeStorageBuffer(RDGBufferHandle handle, vk::PipelineStageFlags stages, vk::AccessFlags access);
    RDGPass &writeTexture(RDGTextureHandle handle, vk::PipelineStageFlags stages, vk::AccessFlags access);
    RDGPass &writeBuffer(RDGBufferHandle handle, vk::PipelineStageFlags stages, vk::AccessFlags access); // Getter 方法
    const std::string &getName() const
    {
        return m_name;
    }
    const std::vector<TextureAccess> &getTextureReads() const
    {
        return m_textureReads;
    }
    const std::vector<BufferAccess> &getBufferReads() const
    {
        return m_bufferReads;
    }
    const std::vector<ColorAttachment> &getColorAttachments() const
    {
        return m_colorAttachments;
    }
    const DepthAttachment &getDepthAttachment() const
    {
        return m_depthAttachment;
    }
    const std::vector<TextureAccess> &getTextureWrites() const
    {
        return m_textureWrites;
    }
    const std::vector<BufferAccess> &getBufferWrites() const
    {
        return m_bufferWrites;
    }

    bool isUsingExtendedCallback() const
    {
        return m_useExtendedCallback;
    }
    const ExecuteCallback &getExecuteCallback() const
    {
        return m_executeCallback;
    }
    const ExecuteCallbackEx &getExecuteCallbackEx() const
    {
        return m_executeCallbackEx;
    }

    // Pass 类型判断
    bool isGraphicsPass() const
    {
        return !m_colorAttachments.empty() || m_depthAttachment.handle.isValid();
    }
    bool isComputePass() const
    {
        return !isGraphicsPass() && (!m_textureWrites.empty() || !m_bufferWrites.empty());
    }

    // 允许 RenderGraph 访问私有成员
    friend class RenderGraph;

  private:
    std::string m_name;
    ExecuteCallback m_executeCallback;
    ExecuteCallbackEx m_executeCallbackEx;
    bool m_useExtendedCallback;

    std::vector<TextureAccess> m_textureReads;
    std::vector<BufferAccess> m_bufferReads;
    std::vector<ColorAttachment> m_colorAttachments;
    DepthAttachment m_depthAttachment;
    std::vector<TextureAccess> m_textureWrites;
    std::vector<BufferAccess> m_bufferWrites;
};

} // namespace rendercore
