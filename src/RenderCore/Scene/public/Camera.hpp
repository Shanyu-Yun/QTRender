#pragma once
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

namespace rendercore
{
/**
 * @file Camera.hpp
 * @brief 管理视图和投影矩阵的摄像机类
 * @details 提供摄像机位置、朝向、投影参数的管理，
 * 并生成用于渲染的 View 和 Projection 矩阵。
 */
class Camera
{
  public:
    enum class CameraMovement
    {
        FORWARD,
        BACKWARD,
        LEFT,
        RIGHT,
        UP,
        DOWN
    };

    Camera(glm::vec3 position = glm::vec3(0.0f, 0.0f, 3.0f), glm::vec3 up = glm::vec3(0.0f, 1.0f, 0.0f),
           float yaw = -90.0f, float pitch = 0.0f);

    ~Camera();

    glm::mat4 getViewMatrix() const;

    const glm::mat4 &getProjectionMatrix() const;

    const glm::vec3 &getPosition() const;

    const glm::vec3 &getFront() const;

    void setPerspective(float fovY, float aspectRatio, float zNear, float zFar);

    void setPosition(const glm::vec3 &position);

    void setRotation(float yaw, float pitch);

    void processKeyboard(CameraMovement direction, float deltaTime);

    void processMouseMovement(float xoffset, float yoffset, bool constrainPitch = true);

    void processMouseScroll(float yoffset);

    void updateAspectRatio(float aspectRatio);

  private:
    void updateCameraVectors();

  private:
    glm::vec3 m_position;
    glm::vec3 m_front;
    glm::vec3 m_up;
    glm::vec3 m_right;
    glm::vec3 m_worldUp;

    float m_yaw;
    float m_pitch;

    float m_movementSpeed;
    float m_mouseSensitivity;
    float m_zoom;

    glm::mat4 m_projectionMatrix;
    float m_aspect;
    float m_zNear;
    float m_zFar;
};

} // namespace rendercore