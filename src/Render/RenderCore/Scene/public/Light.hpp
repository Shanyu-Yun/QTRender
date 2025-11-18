#pragma once

#include <glm/glm.hpp>
#include <memory>
#include <string>
#include <vector>

namespace rendercore
{

/**
 * @enum LightType
 * @brief 光照类型枚举
 */
enum class LightType
{
    Directional, ///< 平行光（如太阳光）
    Point,       ///< 点光源
    Spot,        ///< 聚光灯
    Area         ///< 区域光（未来扩展）
};

/**
 * @class Light
 * @brief 光照基类，定义所有光照的通用属性
 * @details 提供光照的基本属性如颜色、强度等，作为多种光照类型的基类
 */
class Light
{
  public:
    Light(LightType type, const std::string &name = "Light");
    virtual ~Light() = default;

    /** 禁用拷贝与移动（使用智能指针管理） */
    Light(const Light &) = delete;
    Light &operator=(const Light &) = delete;

    // ==================== 基本属性 ====================

    /**
     * @brief 获取光照类型
     */
    LightType getType() const
    {
        return m_type;
    }

    /**
     * @brief 获取光照名称
     */
    const std::string &getName() const
    {
        return m_name;
    }

    /**
     * @brief 设置光照名称
     */
    void setName(const std::string &name)
    {
        m_name = name;
    }

    /**
     * @brief 获取光照颜色
     */
    const glm::vec3 &getColor() const
    {
        return m_color;
    }

    /**
     * @brief 设置光照颜色
     */
    void setColor(const glm::vec3 &color)
    {
        m_color = color;
    }

    /**
     * @brief 获取光照强度
     */
    float getIntensity() const
    {
        return m_intensity;
    }

    /**
     * @brief 设置光照强度
     */
    void setIntensity(float intensity)
    {
        m_intensity = intensity;
    }

    /**
     * @brief 获取是否启用
     */
    bool isEnabled() const
    {
        return m_enabled;
    }

    /**
     * @brief 设置是否启用
     */
    void setEnabled(bool enabled)
    {
        m_enabled = enabled;
    }

    /**
     * @brief 获取是否投射阴影
     */
    bool isCastShadows() const
    {
        return m_castShadows;
    }

    /**
     * @brief 设置是否投射阴影
     */
    void setCastShadows(bool castShadows)
    {
        m_castShadows = castShadows;
    }

    // ==================== 虚函数接口 ====================

    /**
     * @brief 获取光照在世界空间的位置（对于有位置的光源）
     */
    virtual glm::vec3 getWorldPosition() const
    {
        return glm::vec3(0.0f);
    }

    /**
     * @brief 获取光照方向（对于有方向的光源）
     */
    virtual glm::vec3 getDirection() const
    {
        return glm::vec3(0.0f, -1.0f, 0.0f);
    }

    /**
     * @brief 计算指定位置的光照衰减
     */
    virtual float calculateAttenuation([[maybe_unused]] const glm::vec3 &worldPos) const
    {
        return 1.0f;
    }

  protected:
    LightType m_type;
    std::string m_name;
    glm::vec3 m_color{1.0f, 1.0f, 1.0f}; ///< 光照颜色
    float m_intensity{1.0f};             ///< 光照强度
    bool m_enabled{true};                ///< 是否启用
    bool m_castShadows{true};            ///< 是否投射阴影
};

/**
 * @class DirectionalLight
 * @brief 平行光类（如太阳光）
 * @details 平行光没有位置，只有方向，光线平行且不衰减
 */
class DirectionalLight : public Light
{
  public:
    DirectionalLight(const std::string &name = "DirectionalLight");
    ~DirectionalLight() = default;

    /**
     * @brief 获取光照方向
     */
    glm::vec3 getDirection() const override
    {
        return m_direction;
    }

    /**
     * @brief 设置光照方向
     */
    void setDirection(const glm::vec3 &direction);

    /**
     * @brief 平行光不衰减
     */
    float calculateAttenuation([[maybe_unused]] const glm::vec3 &worldPos) const override
    {
        return 1.0f;
    }

  private:
    glm::vec3 m_direction{0.0f, -1.0f, 0.0f}; ///< 光照方向（默认向下）
};

/**
 * @class PointLight
 * @brief 点光源类
 * @details 从一点向四周发散的光源，具有位置和衰减
 */
class PointLight : public Light
{
  public:
    PointLight(const std::string &name = "PointLight");
    ~PointLight() = default;

    /**
     * @brief 获取光源位置
     */
    glm::vec3 getWorldPosition() const override
    {
        return m_position;
    }

    /**
     * @brief 设置光源位置
     */
    void setPosition(const glm::vec3 &position)
    {
        m_position = position;
    }

    /**
     * @brief 计算衰减（基于距离）
     */
    float calculateAttenuation(const glm::vec3 &worldPos) const override;

    /**
     * @brief 设置衰减参数
     */
    void setAttenuation(float constant, float linear, float quadratic);

    /**
     * @brief 获取衰减参数
     */
    glm::vec3 getAttenuation() const
    {
        return glm::vec3(m_constant, m_linear, m_quadratic);
    }

  private:
    glm::vec3 m_position{0.0f, 0.0f, 0.0f}; ///< 光源位置
    float m_constant{1.0f};                 ///< 常数衰减项
    float m_linear{0.09f};                  ///< 线性衰减项
    float m_quadratic{0.032f};              ///< 二次衰减项
};

/**
 * @class SpotLight
 * @brief 聚光灯类
 * @details 从一点向特定方向发射的锥形光源
 */
class SpotLight : public Light
{
  public:
    SpotLight(const std::string &name = "SpotLight");
    ~SpotLight() = default;

    /**
     * @brief 获取光源位置
     */
    glm::vec3 getWorldPosition() const override
    {
        return m_position;
    }

    /**
     * @brief 设置光源位置
     */
    void setPosition(const glm::vec3 &position)
    {
        m_position = position;
    }

    /**
     * @brief 获取光照方向
     */
    glm::vec3 getDirection() const override
    {
        return m_direction;
    }

    /**
     * @brief 设置光照方向
     */
    void setDirection(const glm::vec3 &direction);

    /**
     * @brief 计算衰减（距离 + 角度衰减）
     */
    float calculateAttenuation(const glm::vec3 &worldPos) const override;

    /**
     * @brief 设置聚光灯锥角（以度为单位）
     */
    void setCutoff(float innerCutoff, float outerCutoff);

    /**
     * @brief 设置距离衰减参数
     */
    void setAttenuation(float constant, float linear, float quadratic);

    /**
     * @brief 获取内锥角余弦值
     */
    float getInnerCutoff() const
    {
        return m_innerCutoff;
    }

    /**
     * @brief 获取外锥角余弦值
     */
    float getOuterCutoff() const
    {
        return m_outerCutoff;
    }

    /**
     * @brief 获取衰减参数
     */
    glm::vec3 getAttenuation() const
    {
        return glm::vec3(m_constant, m_linear, m_quadratic);
    }

  private:
    glm::vec3 m_position{0.0f, 0.0f, 0.0f};   ///< 光源位置
    glm::vec3 m_direction{0.0f, -1.0f, 0.0f}; ///< 光照方向
    float m_innerCutoff{0.91f};               ///< 内锥角余弦值 (约25度)
    float m_outerCutoff{0.82f};               ///< 外锥角余弦值 (约35度)

    // 距离衰减参数
    float m_constant{1.0f};
    float m_linear{0.09f};
    float m_quadratic{0.032f};
};

/**
 * @class LightFactory
 * @brief 光照工厂类，提供便捷的光照创建方法
 * @details 封装常用的光照配置，简化光照对象的创建过程
 */
class LightFactory
{
  public:
    // ==================== 创建标准光照 ====================

    /**
     * @brief 创建标准的太阳光（平行光）
     * @param direction 光照方向
     * @param color 光照颜色（默认暖白色）
     * @param intensity 强度（默认1.0）
     * @return DirectionalLight的智能指针
     */
    static std::shared_ptr<DirectionalLight> createSunLight(const glm::vec3 &direction = glm::vec3(0.2f, -1.0f, 0.3f),
                                                            const glm::vec3 &color = glm::vec3(1.0f, 0.95f, 0.8f),
                                                            float intensity = 1.0f);

    /**
     * @brief 创建标准的点光源（如灯泡）
     * @param position 光源位置
     * @param color 光照颜色
     * @param intensity 强度
     * @param range 有效范围（用于计算衰减）
     * @return PointLight的智能指针
     */
    static std::shared_ptr<PointLight> createPointLight(const glm::vec3 &position,
                                                        const glm::vec3 &color = glm::vec3(1.0f, 1.0f, 1.0f),
                                                        float intensity = 1.0f, float range = 10.0f);

    /**
     * @brief 创建标准的聚光灯（如手电筒）
     * @param position 光源位置
     * @param direction 光照方向
     * @param innerCone 内锥角（度）
     * @param outerCone 外锥角（度）
     * @param color 光照颜色
     * @param intensity 强度
     * @param range 有效范围
     * @return SpotLight的智能指针
     */
    static std::shared_ptr<SpotLight> createSpotLight(const glm::vec3 &position, const glm::vec3 &direction,
                                                      float innerCone = 25.0f, float outerCone = 35.0f,
                                                      const glm::vec3 &color = glm::vec3(1.0f, 1.0f, 1.0f),
                                                      float intensity = 1.0f, float range = 15.0f);

    // ==================== 创建预设场景光照 ====================

    /**
     * @brief 创建室外自然光照设置
     * @return 包含太阳光的光照列表
     */
    static std::vector<std::shared_ptr<Light>> createOutdoorLighting();

    /**
     * @brief 创建室内基础光照设置
     * @return 包含环境光和关键光的光照列表
     */
    static std::vector<std::shared_ptr<Light>> createIndoorLighting();

    /**
     * @brief 创建三点光照设置（摄影/渲染常用）
     * @param target 目标位置（物体中心）
     * @param distance 光源距离目标的距离
     * @return 包含关键光、填充光、轮廓光的列表
     */
    static std::vector<std::shared_ptr<Light>> createThreePointLighting(const glm::vec3 &target = glm::vec3(0.0f),
                                                                        float distance = 5.0f);

  private:
    /**
     * @brief 根据范围计算合适的衰减参数
     * @param range 光照的有效范围
     * @return 衰减参数 (constant, linear, quadratic)
     */
    static glm::vec3 calculateAttenuationFromRange(float range);
};

} // namespace rendercore