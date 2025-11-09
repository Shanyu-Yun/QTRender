#include "../public/Scene.hpp"
#include "../../ResourceManager/public/ResourceType.hpp" // 包含Mesh和Material的具体定义
#include "Camera.hpp"                                    // Camera现在在同一模块中
#include "Light.hpp"                                     // 包含Light类
#include <algorithm>
#include <iostream>

namespace rendercore
{

Scene::Scene()
{
    // 创建根节点
    m_rootNode = std::make_shared<SceneNode>("RootNode");
}

Scene::~Scene() = default;

std::shared_ptr<SceneNode> Scene::getRootNode() const
{
    return m_rootNode;
}

void Scene::setCamera(std::shared_ptr<Camera> camera)
{
    m_activeCamera = camera;
}

std::shared_ptr<Camera> Scene::getCamera() const
{
    return m_activeCamera;
}

std::vector<RenderObject> Scene::getRenderObjects()
{
    std::vector<RenderObject> renderList;

    if (m_rootNode)
    {
        traversescene(m_rootNode, renderList);
    }

    return renderList;
}

void Scene::traversescene(std::shared_ptr<SceneNode> node, std::vector<RenderObject> &renderList)
{
    if (!node)
    {
        return;
    }

    // 如果节点有可渲染组件且可见，添加到渲染列表
    if (node->hasRenderable())
    {
        const Renderable &renderable = node->getRenderable();
        if (renderable.visible && renderable.mesh && renderable.material)
        {
            RenderObject renderObject;
            renderObject.renderable = renderable;
            renderObject.worldMatrix = node->getWorldMatrix();
            renderList.push_back(renderObject);
        }
    }

    // 递归遍历所有子节点
    for (const auto &child : node->getChildren())
    {
        traversescene(child, renderList);
    }
}

// ==================== 光照管理接口实现 ====================

uint32_t Scene::addLight(std::shared_ptr<Light> light)
{
    if (!light)
    {
        std::cerr << "Warning: Attempted to add null light to scene" << std::endl;
        return 0;
    }

    uint32_t lightId = m_nextLightId++;
    m_lights[lightId] = light;

    std::cout << "Added "
              << (light->getType() == LightType::Directional ? "Directional"
                  : light->getType() == LightType::Point     ? "Point"
                  : light->getType() == LightType::Spot      ? "Spot"
                                                             : "Unknown")
              << " light '" << light->getName() << "' with ID " << lightId << std::endl;

    return lightId;
}

bool Scene::removeLight(uint32_t lightId)
{
    auto it = m_lights.find(lightId);
    if (it != m_lights.end())
    {
        std::cout << "Removed light '" << it->second->getName() << "' with ID " << lightId << std::endl;
        m_lights.erase(it);
        return true;
    }

    std::cerr << "Warning: Light with ID " << lightId << " not found" << std::endl;
    return false;
}

std::shared_ptr<Light> Scene::findLight(const std::string &name) const
{
    for (const auto &pair : m_lights)
    {
        if (pair.second->getName() == name)
        {
            return pair.second;
        }
    }
    return nullptr;
}

std::shared_ptr<Light> Scene::getLight(uint32_t lightId) const
{
    auto it = m_lights.find(lightId);
    if (it != m_lights.end())
    {
        return it->second;
    }
    return nullptr;
}

std::vector<std::shared_ptr<Light>> Scene::getAllLights() const
{
    std::vector<std::shared_ptr<Light>> lights;
    lights.reserve(m_lights.size());

    for (const auto &pair : m_lights)
    {
        lights.push_back(pair.second);
    }

    return lights;
}

std::vector<std::shared_ptr<Light>> Scene::getLightsByType(LightType type) const
{
    std::vector<std::shared_ptr<Light>> lights;

    for (const auto &pair : m_lights)
    {
        if (pair.second->getType() == type)
        {
            lights.push_back(pair.second);
        }
    }

    return lights;
}

size_t Scene::getActiveLightCount() const
{
    size_t count = 0;
    for (const auto &pair : m_lights)
    {
        if (pair.second->isEnabled())
        {
            ++count;
        }
    }
    return count;
}

void Scene::clearLights()
{
    std::cout << "Cleared " << m_lights.size() << " lights from scene" << std::endl;
    m_lights.clear();
}

} // namespace rendercore