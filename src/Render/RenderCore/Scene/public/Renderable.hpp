#pragma once

#include <memory>

// 前向声明，避免包含依赖问题
namespace rendercore
{
struct Mesh;
struct Material;
} // namespace rendercore

namespace rendercore
{
/**
 * @struct Renderable
 * @brief 可渲染组件，将一个 Mesh 绑定到一个 Material
 */
struct Renderable
{
    std::shared_ptr<Mesh> mesh;
    std::shared_ptr<Material> material;
    bool visible{true};
};
} // namespace rendercore