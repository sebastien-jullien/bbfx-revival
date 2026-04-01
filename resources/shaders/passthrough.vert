#version 330 core
in vec4 vertex;
in vec3 normal;
in vec2 uv0;
uniform mat4 worldViewProj;
uniform mat4 world;
out vec2 oUv0;
out vec3 oNormal;
void main() {
    gl_Position = worldViewProj * vertex;
    oUv0 = uv0;
    oNormal = mat3(world) * normal;
}
