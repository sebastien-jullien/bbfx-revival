#version 330 core
// Flatten vertex shader — squash Y to zero
in vec4 vertex;
in vec3 normal;
in vec2 uv0;
uniform mat4 worldViewProj;
uniform mat4 world;
uniform float flatness;  // 0=normal, 1=flat
out vec2 oUv0;
out vec3 oNormal;

void main() {
    vec4 pos = vertex;
    pos.y = mix(pos.y, 0.0, flatness);
    gl_Position = worldViewProj * pos;
    oUv0 = uv0;
    vec3 n = normal;
    n.y = mix(n.y, 0.0, flatness);
    oNormal = mat3(world) * normalize(n);
}
