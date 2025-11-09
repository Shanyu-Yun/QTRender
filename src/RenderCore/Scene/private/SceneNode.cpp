#include "../public/SceneNode.hpp"
#include <algorithm>
#include <iostream>

namespace rendercore
{

SceneNode::SceneNode(const std::string &name) : m_name(name)
{
}

SceneNode::~SceneNode() = default;

// --- 层次结构 ---

void SceneNode::addChild(std::shared_ptr<SceneNode> child)
{
    if (!child)
    {
        return;
    }

    // 检查是否已经是子节点
    auto it = std::find(m_children.begin(), m_children.end(), child);
    if (it != m_children.end())
    {
        return; // 已经是子节点，直接返回
    }

    // 如果子节点已经有父节点，先从旧父节点移除
    if (auto oldParent = child->getParent().lock())
    {
        oldParent->removeChild(child);
    }

    // 添加为子节点
    m_children.push_back(child);
    child->setparent(weak_from_this());

    // 子节点的世界矩阵需要重新计算
    child->invalidateworldmatrix();
}

void SceneNode::removeChild(std::shared_ptr<SceneNode> child)
{
    if (!child)
    {
        return;
    }

    auto it = std::find(m_children.begin(), m_children.end(), child);
    if (it != m_children.end())
    {
        (*it)->setparent(std::weak_ptr<SceneNode>{});
        m_children.erase(it);

        // 子节点的世界矩阵需要重新计算
        child->invalidateworldmatrix();
    }
}

std::weak_ptr<SceneNode> SceneNode::getParent() const
{
    return m_parent;
}

const std::vector<std::shared_ptr<SceneNode>> &SceneNode::getChildren() const
{
    return m_children;
}

// --- 变换访问器 (只读) ---

const Transform &SceneNode::getTransform() const
{
    return m_transform;
}

const glm::vec3 &SceneNode::getPosition() const
{
    return m_transform.position;
}

const glm::quat &SceneNode::getRotation() const
{
    return m_transform.rotation;
}

const glm::vec3 &SceneNode::getScale() const
{
    return m_transform.scale;
}

// --- 变换修改器 (自动处理脏标记) ---

void SceneNode::setTransform(const Transform &transform)
{
    m_transform = transform;
    invalidateworldmatrix();
}

void SceneNode::setPosition(const glm::vec3 &position)
{
    m_transform.position = position;
    invalidateworldmatrix();
}

void SceneNode::setRotation(const glm::quat &rotation)
{
    m_transform.rotation = rotation;
    invalidateworldmatrix();
}

void SceneNode::setScale(const glm::vec3 &scale)
{
    m_transform.scale = scale;
    invalidateworldmatrix();
}

void SceneNode::translate(const glm::vec3 &delta)
{
    m_transform.position += delta;
    invalidateworldmatrix();
}

glm::mat4 SceneNode::getWorldMatrix()
{
    // 如果世界矩阵没有脏，直接返回缓存的结果
    if (!m_worldMatrixDirty)
    {
        return m_cachedWorldMatrix;
    }

    // 重新计算世界矩阵
    glm::mat4 localMatrix = m_transform.getLocalMatrix();

    if (auto parent = m_parent.lock())
    {
        // 有父节点：世界矩阵 = 父世界矩阵 * 局部矩阵
        m_cachedWorldMatrix = parent->getWorldMatrix() * localMatrix;
    }
    else
    {
        // 根节点：世界矩阵 = 局部矩阵
        m_cachedWorldMatrix = localMatrix;
    }

    // 清除脏标记
    m_worldMatrixDirty = false;

    return m_cachedWorldMatrix;
}

// --- 组件 ---

void SceneNode::setRenderable(const Renderable &renderable)
{
    m_renderable = renderable;
    m_hasRenderable = true;
}

Renderable &SceneNode::getRenderable()
{
    return m_renderable;
}

bool SceneNode::hasRenderable() const
{
    return m_hasRenderable;
}

const std::string &SceneNode::getName() const
{
    return m_name;
}

// --- 私有方法 ---

void SceneNode::setparent(std::weak_ptr<SceneNode> parent)
{
    m_parent = parent;
    invalidateworldmatrix();
}

void SceneNode::invalidateworldmatrix()
{
    // 标记自身为脏
    m_worldMatrixDirty = true;

    // 递归标记所有子节点为脏
    for (auto &child : m_children)
    {
        child->invalidateworldmatrix();
    }
}

} // namespace rendercore