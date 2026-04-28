#version 330 core
// Inflate vertex shader — uniform expansion along normals
in vec4 vertex;
in vec3 normal;
in vec2 uv0;
uniform mat4 worldViewProj;
uniform mat4 world;
uniform float amount;  // def 0.0
out vec2 oUv0;
out vec3 oNormal;

void main() {
    vec4 pos = vertex;
    pos.xyz += normal * amount;
    gl_Position = worldViewProj * pos;
    oUv0 = uv0;
    oNormal = mat3(world) * normal;
}
