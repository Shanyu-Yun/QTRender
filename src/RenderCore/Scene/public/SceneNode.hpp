#pragma once

#include "Renderable.hpp"
#include "Transform.hpp"
#include <memory>
#include <string>
#include <vector>

namespace rendercore
{
class Scene; // 前向声明

/**
 * @class SceneNode
 * @brief 场景图中的一个节点，包含变换、子节点和组件
 * @details 遵循代码规范，公共函数 camelCase，私有成员 m_lowercase
 */
class SceneNode : public std::enable_shared_from_this<SceneNode>
{
  public:
    /**
     * @brief 构造函数
     * @param name 节点名称 (用于调试)
     */
    SceneNode(const std::string &name = "SceneNode");
    ~SceneNode();

    /** 禁用拷贝与移动 */
    SceneNode(const SceneNode &) = delete;
    SceneNode &operator=(const SceneNode &) = delete;

    // --- 层次结构 ---

    /**
     * @brief 添加一个子节点
     * @param child 要添加的子节点
     */
    void addChild(std::shared_ptr<SceneNode> child);

    /**
     * @brief 移除一个子节点
     * @param child 要移除的子节点
     */
    void removeChild(std::shared_ptr<SceneNode> child);

    /**
     * @brief 获取此节点的父节点
     */
    std::weak_ptr<SceneNode> getParent() const;

    /**
     * @brief 获取子节点列表 (const 引用)
     */
    const std::vector<std::shared_ptr<SceneNode>> &getChildren() const;

    // --- 变换 ---

    /**
     * @brief 获取此节点的局部变换 (可修改)
     */
    Transform &getTransform();

    /**
     * @brief 获取此节点的最终世界变换矩阵 (只读)
     * @details 递归地与其父节点的世界矩阵相乘，并缓存结果
     */
    glm::mat4 getWorldMatrix();

    // --- 组件 ---

    /**
     * @brief 设置此节点的可渲染组件 (Mesh + Material)
     * @param renderable Renderable 结构体
     */
    void setRenderable(const Renderable &renderable);

    /**
     * @brief 获取此节点的可渲染组件 (可修改)
     */
    Renderable &getRenderable();

    /**
     * @brief 检查此节点是否有可渲染组件
     */
    bool hasRenderable() const;

    /**
     * @brief 获取节点名称
     */
    const std::string &getName() const;

  private:
    /**
     * @brief (私有) 设置父节点，由 addChild 自动调用
     */
    void setparent(std::weak_ptr<SceneNode> parent);

    /**
     * @brief (私有) 标记世界矩阵为脏，并递归通知所有子节点
     */
    void invalidateworldmatrix();

  private:
    std::string m_name;
    Transform m_transform;
    std::weak_ptr<SceneNode> m_parent;
    std::vector<std::shared_ptr<SceneNode>> m_children;

    // 组件
    Renderable m_renderable;
    bool m_hasRenderable{false};

    // 缓存
    bool m_worldMatrixDirty{true};
    glm::mat4 m_cachedWorldMatrix{1.0f};
};
} // namespace rendercore