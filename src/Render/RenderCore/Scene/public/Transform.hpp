#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>

namespace rendercore
{
/**
 * @struct Transform
 * @brief SceneNode 的变换组件，包含位置、旋转和缩放。
 * @details 遵循代码规范，公开成员使用 camelCase
 */
struct Transform
{
    glm::vec3 position{0.0f, 0.0f, 0.0f};
    glm::quat rotation{1.0f, 0.0f, 0.0f, 0.0f}; // W, X, Y, Z
    glm::vec3 scale{1.0f, 1.0f, 1.0f};

    /**
     * @brief 计算此变换的局部模型矩阵
     * @return glm::mat4 4x4 模型矩阵
     */
    glm::mat4 getLocalMatrix() const
    {
        glm::mat4 trans = glm::translate(glm::mat4(1.0f), position);
        glm::mat4 rot = glm::mat4_cast(rotation);
        glm::mat4 sc = glm::scale(glm::mat4(1.0f), scale);
        return trans * rot * sc;
    }
};
} // namespace rendercore