#version 330 core
in vec4 vertex;
in vec3 normal;
in vec2 uv0;
uniform mat4 worldViewProj;
uniform mat4 world;
uniform float time;
uniform float amplitude;
uniform float frequency;
uniform float speed;
out vec2 oUv0;
out vec3 oNormal;
void main() {
    vec3 pos = vertex.xyz;
    pos.y += amplitude * sin(frequency * pos.x + speed * time);
    gl_Position = worldViewProj * vec4(pos, 1.0);
    oUv0 = uv0;
    oNormal = mat3(world) * normal;
}
