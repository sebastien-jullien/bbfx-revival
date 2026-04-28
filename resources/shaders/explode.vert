#version 330 core
// Explode vertex shader — expansion along normals with noise
in vec4 vertex;
in vec3 normal;
in vec2 uv0;
uniform mat4 worldViewProj;
uniform mat4 world;
uniform float intensity;  // def 0.5
uniform float time;
out vec2 oUv0;
out vec3 oNormal;

float hash(vec3 p) {
    float h = dot(p, vec3(127.1, 311.7, 74.7));
    return fract(sin(h) * 43758.5453);
}

float noise3d(vec3 p) {
    vec3 i = floor(p);
    vec3 f = fract(p);
    f = f * f * (3.0 - 2.0 * f);
    float a = hash(i);
    float b = hash(i + vec3(1, 0, 0));
    float c = hash(i + vec3(0, 1, 0));
    float d = hash(i + vec3(1, 1, 0));
    float e = hash(i + vec3(0, 0, 1));
    float ff = hash(i + vec3(1, 0, 1));
    float g = hash(i + vec3(0, 1, 1));
    float h2 = hash(i + vec3(1, 1, 1));
    float x1 = mix(a, b, f.x);
    float x2 = mix(c, d, f.x);
    float x3 = mix(e, ff, f.x);
    float x4 = mix(g, h2, f.x);
    float y1 = mix(x1, x2, f.y);
    float y2 = mix(x3, x4, f.y);
    return mix(y1, y2, f.z) * 2.0 - 1.0;
}

void main() {
    vec4 pos = vertex;
    float n = noise3d(pos.xyz * 2.0 + time * 0.5);
    pos.xyz += normal * intensity * n;
    gl_Position = worldViewProj * pos;
    oUv0 = uv0;
    oNormal = mat3(world) * normal;
}
