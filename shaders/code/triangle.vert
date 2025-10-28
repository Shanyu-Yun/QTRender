#version 450
#extension GL_ARB_separate_shader_objects : enable

// 三角形的三个顶点（硬编码在着色器中）
vec2 positions[3] = vec2[](
    vec2(0.0, -0.5), // 顶部
    vec2(0.5, 0.5), // 右下
    vec2(-0.5, 0.5) // 左下
);

// 三角形的三个颜色（RGB）
vec3 colors[3] = vec3[](
    vec3(1.0, 0.0, 0.0), // 红色
    vec3(0.0, 1.0, 0.0), // 绿色
    vec3(0.0, 0.0, 1.0) // 蓝色
);

// 输出到片段着色器
layout(location = 0) out vec3 fragColor;

void main()
{
    gl_Position = vec4(positions[gl_VertexIndex], 0.0, 1.0);
    fragColor = colors[gl_VertexIndex];
}
