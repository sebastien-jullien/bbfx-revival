#version 330 core
// Audio pulse vertex shader — displacement driven by frequency bands
in vec4 vertex;
in vec3 normal;
in vec2 uv0;
uniform mat4 worldViewProj;
uniform mat4 world;
uniform float bass;   // def 0.0
uniform float mid;    // def 0.0
uniform float high;   // def 0.0
out vec2 oUv0;
out vec3 oNormal;

void main() {
    vec4 pos = vertex;
    float displacement = bass * 0.5 + mid * 0.3 + high * 0.2;
    pos.xyz += normal * displacement;
    gl_Position = worldViewProj * pos;
    oUv0 = uv0;
    oNormal = mat3(world) * normal;
}
