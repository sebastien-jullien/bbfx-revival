#version 330 core

in vec4 vertex;
in vec2 uv0;

out vec2 oUv0;

uniform mat4 worldViewProj;

void main()
{
    gl_Position = worldViewProj * vertex;
    oUv0 = uv0;
}
