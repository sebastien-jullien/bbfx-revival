#version 330 core
in vec4 vertex;
in vec3 normal;
in vec2 uv0;
uniform mat4 worldViewProj;
uniform mat4 world;
uniform float time;
uniform float displacement;
uniform float density;
uniform float speed;
out vec2 oUv0;
out vec3 oNormal;
// Simplified Perlin-like noise
vec3 mod289(vec3 x) { return x - floor(x * (1.0 / 289.0)) * 289.0; }
vec4 mod289(vec4 x) { return x - floor(x * (1.0 / 289.0)) * 289.0; }
vec4 permute(vec4 x) { return mod289(((x*34.0)+1.0)*x); }
float snoise(vec3 v) {
    vec3 i = floor(v + dot(v, vec3(0.333333)));
    vec3 x0 = v - i + dot(i, vec3(0.166667));
    vec3 g = step(x0.yzx, x0.xyz);
    vec3 l = 1.0 - g;
    vec3 i1 = min(g, l.zxy);
    vec3 i2 = max(g, l.zxy);
    vec3 x1 = x0 - i1 + 0.166667;
    vec3 x2 = x0 - i2 + 0.333333;
    vec3 x3 = x0 - 0.5;
    vec4 w = max(0.6 - vec4(dot(x0,x0), dot(x1,x1), dot(x2,x2), dot(x3,x3)), 0.0);
    w = w * w * w * w;
    return dot(w, vec4(dot(x0, x0.yzx), dot(x1, x1.yzx), dot(x2, x2.yzx), dot(x3, x3.yzx))) * 32.0;
}
void main() {
    float n = snoise(vertex.xyz / density + time * speed);
    vec3 displaced = vertex.xyz + normal * n * displacement;
    gl_Position = worldViewProj * vec4(displaced, 1.0);
    oUv0 = uv0;
    oNormal = mat3(world) * normal;
}
