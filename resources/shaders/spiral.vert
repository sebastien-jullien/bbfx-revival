#version 330 core
// Spiral vertex shader — helical twist around Y axis
in vec4 vertex;
in vec3 normal;
in vec2 uv0;
uniform mat4 worldViewProj;
uniform mat4 world;
uniform float twist;  // def 1.0
uniform float time;
out vec2 oUv0;
out vec3 oNormal;

void main() {
    vec4 pos = vertex;
    float angle = pos.y * twist + time * 0.5;
    float c = cos(angle);
    float s = sin(angle);
    float x = pos.x * c - pos.z * s;
    float z = pos.x * s + pos.z * c;
    pos.x = x;
    pos.z = z;
    // Also twist the normal
    vec3 n = normal;
    float nx = n.x * c - n.z * s;
    float nz = n.x * s + n.z * c;
    n.x = nx;
    n.z = nz;
    gl_Position = worldViewProj * pos;
    oUv0 = uv0;
    oNormal = mat3(world) * n;
}
