#include "Camera.hpp"

namespace rendercore
{
Camera::Camera(glm::vec3 position, glm::vec3 up, float yaw, float pitch)
    : m_position(position), m_worldUp(up), m_yaw(yaw), m_pitch(pitch), m_movementSpeed(2.5f), m_mouseSensitivity(0.1f),
      m_zoom(45.0f), m_aspect(16.0f / 9.0f), m_zNear(0.1f), m_zFar(100.0f)
{
    // m_front 通过 updateCameraVectors() 根据 yaw 和 pitch 计算得出
    updateCameraVectors();
    setPerspective(m_zoom, m_aspect, m_zNear, m_zFar);
}

Camera::~Camera()
{
}

glm::mat4 Camera::getViewMatrix() const
{
    return glm::lookAt(m_position, m_position + m_front, m_up);
}

const glm::mat4 &Camera::getProjectionMatrix() const
{
    return m_projectionMatrix;
}

const glm::vec3 &Camera::getPosition() const
{
    return m_position;
}

const glm::vec3 &Camera::getFront() const
{
    return m_front;
}

void Camera::setPerspective(float fovY, float aspectRatio, float zNear, float zFar)
{
    m_projectionMatrix = glm::perspective(glm::radians(fovY), aspectRatio, zNear, zFar);
    m_aspect = aspectRatio;
    m_zNear = zNear;
    m_zFar = zFar;
}

void Camera::setPosition(const glm::vec3 &position)
{
    m_position = position;
}

void Camera::setRotation(float yaw, float pitch)
{
    m_yaw = yaw;
    m_pitch = pitch;
    updateCameraVectors();
}

void Camera::processKeyboard(CameraMovement direction, float deltaTime)
{
    float velocity = m_movementSpeed * deltaTime;
    if (direction == CameraMovement::FORWARD)
        m_position += m_front * velocity;
    if (direction == CameraMovement::BACKWARD)
        m_position -= m_front * velocity;
    if (direction == CameraMovement::LEFT)
        m_position -= m_right * velocity;
    if (direction == CameraMovement::RIGHT)
        m_position += m_right * velocity;
    if (direction == CameraMovement::UP)
        m_position += m_up * velocity;
    if (direction == CameraMovement::DOWN)
        m_position -= m_up * velocity;
}

void Camera::processMouseMovement(float xoffset, float yoffset, bool constrainPitch)
{
    xoffset *= m_mouseSensitivity;
    yoffset *= m_mouseSensitivity;

    m_yaw += xoffset;
    m_pitch += yoffset;

    if (constrainPitch)
    {
        if (m_pitch > 89.0f)
            m_pitch = 89.0f;
        if (m_pitch < -89.0f)
            m_pitch = -89.0f;
    }

    updateCameraVectors();
}

void Camera::processMouseScroll(float yoffset)
{
    m_zoom -= yoffset;
    if (m_zoom < 1.0f)
        m_zoom = 1.0f;
    if (m_zoom > 45.0f)
        m_zoom = 45.0f;
    setPerspective(m_zoom, m_aspect, m_zNear, m_zFar);
}

void Camera::updateAspectRatio(float aspectRatio)
{
    m_aspect = aspectRatio;
    setPerspective(m_zoom, m_aspect, m_zNear, m_zFar);
}

void Camera::updateCameraVectors()
{
    glm::vec3 front;
    front.x = cos(glm::radians(m_yaw)) * cos(glm::radians(m_pitch));
    front.y = sin(glm::radians(m_pitch));
    front.z = sin(glm::radians(m_yaw)) * cos(glm::radians(m_pitch));
    m_front = glm::normalize(front);
    m_right = glm::normalize(glm::cross(m_front, m_worldUp));
    m_up = glm::normalize(glm::cross(m_right, m_front));
}

} // namespace rendercore