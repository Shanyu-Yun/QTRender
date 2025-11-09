#pragma once

#include "RenderCore/Camera/public/Camera.hpp" // 依赖 Camera 模块
#include "SceneNode.hpp"
#include <memory>
#include <vector>

namespace rendercore
{
/**
 * @struct RenderObject
 * @brief 包含渲染所需的所有信息的扁平结构
 * @details 由 Scene 遍历场景图后生成，供渲染器使用
 */
struct RenderObject
{
    Renderable renderable; // 包含 Mesh 和 Material 的引用
    glm::mat4 worldMatrix; // 最终的世界变换矩阵
    // (未来可以添加：包围盒、与此对象相关的灯光列表等)
};

/**
 * @class Scene
 * @brief 场景的根对象，管理场景图和相机
 */
class Scene
{
  public:
    Scene();
    ~Scene();

    /** 禁用拷贝与移动 */
    Scene(const Scene &) = delete;
    Scene &operator=(const Scene &) = delete;

    /**
     * @brief 获取场景的根节点
     */
    std::shared_ptr<SceneNode> getRootNode() const;

    /**
     * @brief 设置场景的活动相机
     * @param camera Camera 对象的共享指针
     */
    void setCamera(std::shared_ptr<Camera> camera);

    /**
     * @brief 获取场景的活动相机
     */
    std::shared_ptr<Camera> getCamera() const;

    /**
     * @brief 遍历场景图，收集所有可见的 Renderable 对象
     * @return std::vector<RenderObject> 供渲染器使用的扁平对象列表 (渲染队列)
     */
    std::vector<RenderObject> getRenderObjects();

  private:
    /**
     * @brief (私有) 递归遍历场景图以收集渲染对象
     * @param node 当前遍历的节点
     * @param renderList (输出) 收集到的渲染对象列表
     */
    void traversescene(std::shared_ptr<SceneNode> node, std::vector<RenderObject> &renderList);

  private:
    std::shared_ptr<SceneNode> m_rootNode;
    std::shared_ptr<Camera> m_activeCamera;
};
} // namespace rendercore