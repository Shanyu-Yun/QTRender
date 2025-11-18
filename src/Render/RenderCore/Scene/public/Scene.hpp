#pragma once

#include "Camera.hpp"
#include "Light.hpp"
#include "SceneNode.hpp"
#include <memory>
#include <unordered_map>
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

    // ==================== 光照管理接口 ====================

    /**
     * @brief 添加光照到场景
     * @param light 光照对象的共享指针
     * @return 光照的唯一ID
     */
    uint32_t addLight(std::shared_ptr<Light> light);

    /**
     * @brief 移除指定ID的光照
     * @param lightId 光照ID
     * @return 是否成功移除
     */
    bool removeLight(uint32_t lightId);

    /**
     * @brief 通过名称查找光照
     * @param name 光照名称
     * @return 光照对象的共享指针，未找到返回nullptr
     */
    std::shared_ptr<Light> findLight(const std::string &name) const;

    /**
     * @brief 通过ID获取光照
     * @param lightId 光照ID
     * @return 光照对象的共享指针，未找到返回nullptr
     */
    std::shared_ptr<Light> getLight(uint32_t lightId) const;

    /**
     * @brief 获取所有光照
     * @return 所有光照对象的列表
     */
    std::vector<std::shared_ptr<Light>> getAllLights() const;

    /**
     * @brief 获取指定类型的所有光照
     * @param type 光照类型
     * @return 指定类型的光照列表
     */
    std::vector<std::shared_ptr<Light>> getLightsByType(LightType type) const;

    /**
     * @brief 获取活跃（启用）的光照数量
     */
    size_t getActiveLightCount() const;

    /**
     * @brief 清除所有光照
     */
    void clearLights();

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

    // 光照管理
    std::unordered_map<uint32_t, std::shared_ptr<Light>> m_lights; ///< 光照ID到光照对象的映射
    uint32_t m_nextLightId{1};                                     ///< 下一个可用的光照ID
};
} // namespace rendercore