/**
 * @file Renderer.hpp
 * @brief 渲染器基础接口
 * @details 定义渲染器的抽象接口，用于实现不同的渲染策略
 */

#pragma once

#include <memory>

namespace renderer
{

/**
 * @class IRenderer
 * @brief 渲染器抽象接口
 */
class IRenderer
{
  public:
    virtual ~IRenderer() = default;

    /**
     * @brief 初始化渲染器
     * @return 是否初始化成功
     */
    virtual bool initialize() = 0;

    /**
     * @brief 渲染一帧
     */
    virtual void render() = 0;

    /**
     * @brief 清理渲染器资源
     */
    virtual void cleanup() = 0;

    /**
     * @brief 获取帧计数
     * @return 当前帧数
     */
    virtual uint64_t getFrameCount() const = 0;
};

} // namespace renderer
