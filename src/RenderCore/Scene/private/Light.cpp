#include "../public/Light.hpp"
#include <algorithm>
#include <cmath>
#include <glm/gtc/matrix_transform.hpp>

namespace rendercore
{

// ==================== Light基类实现 ====================

Light::Light(LightType type, const std::string &name) : m_type(type), m_name(name)
{
}

// ==================== DirectionalLight实现 ====================

DirectionalLight::DirectionalLight(const std::string &name) : Light(LightType::Directional, name)
{
}

void DirectionalLight::setDirection(const glm::vec3 &direction)
{
    m_direction = glm::normalize(direction);
}

// ==================== PointLight实现 ====================

PointLight::PointLight(const std::string &name) : Light(LightType::Point, name)
{
}

float PointLight::calculateAttenuation(const glm::vec3 &worldPos) const
{
    float distance = glm::length(worldPos - m_position);

    // 避免除零错误
    if (distance < 0.001f)
    {
        return 1.0f;
    }

    // 计算衰减：1.0 / (constant + linear * distance + quadratic * distance^2)
    float attenuation = 1.0f / (m_constant + m_linear * distance + m_quadratic * distance * distance);

    return std::max(attenuation, 0.0f);
}

void PointLight::setAttenuation(float constant, float linear, float quadratic)
{
    m_constant = std::max(constant, 0.0f);
    m_linear = std::max(linear, 0.0f);
    m_quadratic = std::max(quadratic, 0.0f);
}

// ==================== SpotLight实现 ====================

SpotLight::SpotLight(const std::string &name) : Light(LightType::Spot, name)
{
}

void SpotLight::setDirection(const glm::vec3 &direction)
{
    m_direction = glm::normalize(direction);
}

float SpotLight::calculateAttenuation(const glm::vec3 &worldPos) const
{
    // 1. 计算距离衰减
    float distance = glm::length(worldPos - m_position);

    if (distance < 0.001f)
    {
        return 1.0f;
    }

    float distanceAttenuation = 1.0f / (m_constant + m_linear * distance + m_quadratic * distance * distance);

    // 2. 计算角度衰减
    glm::vec3 lightToFragment = glm::normalize(worldPos - m_position);
    float cosTheta = glm::dot(lightToFragment, m_direction);

    // 使用平滑过渡（smoothstep）
    float epsilon = m_innerCutoff - m_outerCutoff;
    float spotIntensity = glm::clamp((cosTheta - m_outerCutoff) / epsilon, 0.0f, 1.0f);

    return std::max(distanceAttenuation * spotIntensity, 0.0f);
}

void SpotLight::setCutoff(float innerCutoffDegrees, float outerCutoffDegrees)
{
    // 将角度转换为余弦值
    m_innerCutoff = std::cos(glm::radians(std::clamp(innerCutoffDegrees, 0.0f, 90.0f)));
    m_outerCutoff = std::cos(glm::radians(std::clamp(outerCutoffDegrees, 0.0f, 90.0f)));

    // 确保内锥角小于外锥角
    if (m_innerCutoff < m_outerCutoff)
    {
        std::swap(m_innerCutoff, m_outerCutoff);
    }
}

void SpotLight::setAttenuation(float constant, float linear, float quadratic)
{
    m_constant = std::max(constant, 0.0f);
    m_linear = std::max(linear, 0.0f);
    m_quadratic = std::max(quadratic, 0.0f);
}

// ==================== 创建标准光照 ====================

std::shared_ptr<DirectionalLight> LightFactory::createSunLight(const glm::vec3 &direction, const glm::vec3 &color,
                                                               float intensity)
{
    auto light = std::make_shared<DirectionalLight>("SunLight");
    light->setDirection(direction);
    light->setColor(color);
    light->setIntensity(intensity);
    light->setCastShadows(true);
    return light;
}

std::shared_ptr<PointLight> LightFactory::createPointLight(const glm::vec3 &position, const glm::vec3 &color,
                                                           float intensity, float range)
{
    auto light = std::make_shared<PointLight>("PointLight");
    light->setPosition(position);
    light->setColor(color);
    light->setIntensity(intensity);

    // 根据范围计算衰减参数
    glm::vec3 attenuation = calculateAttenuationFromRange(range);
    light->setAttenuation(attenuation.x, attenuation.y, attenuation.z);

    return light;
}

std::shared_ptr<SpotLight> LightFactory::createSpotLight(const glm::vec3 &position, const glm::vec3 &direction,
                                                         float innerCone, float outerCone, const glm::vec3 &color,
                                                         float intensity, float range)
{
    auto light = std::make_shared<SpotLight>("SpotLight");
    light->setPosition(position);
    light->setDirection(direction);
    light->setColor(color);
    light->setIntensity(intensity);
    light->setCutoff(innerCone, outerCone);

    // 根据范围计算衰减参数
    glm::vec3 attenuation = calculateAttenuationFromRange(range);
    light->setAttenuation(attenuation.x, attenuation.y, attenuation.z);

    return light;
}

// ==================== 创建预设场景光照 ====================

std::vector<std::shared_ptr<Light>> LightFactory::createOutdoorLighting()
{
    std::vector<std::shared_ptr<Light>> lights;

    // 主太阳光
    auto sunLight = createSunLight(glm::vec3(0.3f, -0.8f, 0.5f), // 斜射方向
                                   glm::vec3(1.0f, 0.95f, 0.8f), // 暖色调
                                   1.2f                          // 较强强度
    );
    sunLight->setName("MainSunLight");
    lights.push_back(sunLight);

    // 天空散射光（模拟）
    auto skyLight = createSunLight(glm::vec3(0.0f, -1.0f, 0.0f), // 垂直向下
                                   glm::vec3(0.5f, 0.7f, 1.0f),  // 冷蓝色调
                                   0.3f                          // 较弱强度
    );
    skyLight->setName("SkyLight");
    skyLight->setCastShadows(false); // 天空光不投射阴影
    lights.push_back(skyLight);

    return lights;
}

std::vector<std::shared_ptr<Light>> LightFactory::createIndoorLighting()
{
    std::vector<std::shared_ptr<Light>> lights;

    // 天花板中央的主光源
    auto mainLight = createPointLight(glm::vec3(0.0f, 4.0f, 0.0f),  // 位于上方
                                      glm::vec3(1.0f, 0.95f, 0.9f), // 暖白色
                                      1.0f,                         // 标准强度
                                      12.0f                         // 大范围
    );
    mainLight->setName("MainIndoorLight");
    lights.push_back(mainLight);

    // 角落的辅助光源
    auto fillLight = createPointLight(glm::vec3(-3.0f, 2.5f, -3.0f), // 角落位置
                                      glm::vec3(0.8f, 0.9f, 1.0f),   // 冷白色
                                      0.5f,                          // 较低强度
                                      8.0f                           // 中等范围
    );
    fillLight->setName("FillLight");
    lights.push_back(fillLight);

    return lights;
}

std::vector<std::shared_ptr<Light>> LightFactory::createThreePointLighting(const glm::vec3 &target, float distance)
{
    std::vector<std::shared_ptr<Light>> lights;

    // 1. 关键光（Key Light）- 主光源
    auto keyLight = createSpotLight(
        target + glm::vec3(distance * 0.7f, distance * 0.5f, distance * 0.7f),
        glm::normalize(target - (target + glm::vec3(distance * 0.7f, distance * 0.5f, distance * 0.7f))), 20.0f,
        30.0f,                        // 聚光范围
        glm::vec3(1.0f, 0.95f, 0.9f), // 暖白色
        1.5f,                         // 高强度
        distance * 2.0f);
    keyLight->setName("KeyLight");
    lights.push_back(keyLight);

    // 2. 填充光（Fill Light）- 柔化阴影
    auto fillLight = createSpotLight(
        target + glm::vec3(-distance * 0.5f, distance * 0.3f, distance * 0.8f),
        glm::normalize(target - (target + glm::vec3(-distance * 0.5f, distance * 0.3f, distance * 0.8f))), 25.0f,
        40.0f,                        // 更大聚光范围
        glm::vec3(0.9f, 0.95f, 1.0f), // 冷白色
        0.6f,                         // 中等强度
        distance * 1.8f);
    fillLight->setName("FillLight");
    fillLight->setCastShadows(false); // 填充光通常不投射阴影
    lights.push_back(fillLight);

    // 3. 轮廓光（Rim Light）- 突出边缘
    auto rimLight = createSpotLight(
        target + glm::vec3(distance * 0.2f, distance * 0.8f, -distance * 0.9f),
        glm::normalize(target - (target + glm::vec3(distance * 0.2f, distance * 0.8f, -distance * 0.9f))), 15.0f,
        25.0f,                       // 较小聚光范围
        glm::vec3(1.0f, 1.0f, 0.9f), // 略带暖色
        1.0f,                        // 标准强度
        distance * 1.5f);
    rimLight->setName("RimLight");
    lights.push_back(rimLight);

    return lights;
}

// ==================== 私有辅助方法 ====================

glm::vec3 LightFactory::calculateAttenuationFromRange(float range)
{
    // 基于经验公式计算衰减参数，使光照在指定范围处衰减到约5%
    float constant = 1.0f;
    float linear = 2.0f / range;
    float quadratic = 1.0f / (range * range);

    return glm::vec3(constant, linear, quadratic);
}

} // namespace rendercore