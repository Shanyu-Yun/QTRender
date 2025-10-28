#version 450

// 从顶点着色器输入
layout(location = 0) in vec2 fragTexCoord;

// 纹理采样器（通过 Descriptor Set 绑定）
layout(binding = 0) uniform sampler2D texSampler;

// 输出颜色
layout(location = 0) out vec4 outColor;

void main()
{
    // 从纹理采样颜色
    outColor = texture(texSampler, fragTexCoord);
}
