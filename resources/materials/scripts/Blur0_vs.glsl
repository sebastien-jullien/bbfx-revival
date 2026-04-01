#version 330 core

in vec4 vertex;
in vec2 uv0;

uniform mat4 worldViewProj;

out vec2 oUv0;
out vec2 oUv1;
out vec2 oUv2;
out vec2 oUv3;

void main()
{
    gl_Position = worldViewProj * vertex;
    float texelSize = 1.0 / 128.0;
    oUv0 = uv0 + vec2(0.0, -texelSize);
    oUv1 = uv0;
    oUv2 = uv0 + vec2(0.0,  texelSize);
    oUv3 = uv0 + vec2(0.0,  texelSize * 2.0);
}
