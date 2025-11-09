#pragma once

#include "RenderCore/ResourceManager/public/ResourceType.hpp" // 依赖 Mesh 和 Material
#include <memory>

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