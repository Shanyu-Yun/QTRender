#version 450
#extension GL_ARB_separate_shader_objects : enable

// 三角形的三个顶点（硬编码在着色器中）
vec2 positions[3] = vec2[](
    vec2(0.0, -0.5), // 顶部
    vec2(0.5, 0.5), // 右下
    vec2(-0.5, 0.5) // 左下
);

// UV 坐标（用于纹理采样）
vec2 texCoords[3] = vec2[](
    vec2(0.5, 0.0), // 顶部中心
    vec2(1.0, 1.0), // 右下
    vec2(0.0, 1.0) // 左下
);

// 输出到片段着色器
layout(location = 0) out vec2 fragTexCoord;

void main()
{
    gl_Position = vec4(positions[gl_VertexIndex], 0.0, 1.0);
    fragTexCoord = texCoords[gl_VertexIndex];
}
