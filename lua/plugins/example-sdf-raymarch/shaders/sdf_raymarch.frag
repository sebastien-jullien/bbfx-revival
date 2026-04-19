// ============================================================================
// BBFx v3.5 example plugin — SDF Raymarch fragment shader
// ============================================================================
#version 330 core

in  vec2 vUV;
out vec4 fragColor;

uniform float uTime;
uniform int   uShape;    // 0 sphere / 1 box / 2 torus
uniform float uDistance;
uniform float uSoftness;

float sdSphere(vec3 p, vec3 c, float r) { return length(p - c) - r; }
float sdBox   (vec3 p, vec3 c, vec3 h)  {
    vec3 q = abs(p - c) - h;
    return length(max(q, 0.0)) + min(max(q.x, max(q.y, q.z)), 0.0);
}
float sdTorus (vec3 p, vec3 c, float rMaj, float rMin) {
    vec3 q = p - c;
    vec2 t = vec2(length(q.xz) - rMaj, q.y);
    return length(t) - rMin;
}
float opSmoothUnion(float a, float b, float k) {
    float h = clamp(0.5 + 0.5 * (b - a) / k, 0.0, 1.0);
    return mix(b, a, h) - k * h * (1.0 - h);
}

float map(vec3 p) {
    if (uShape == 1) return sdBox   (p, vec3(0.0), vec3(0.5));
    if (uShape == 2) return sdTorus (p, vec3(0.0), 0.7, 0.2);
    return opSmoothUnion(
        sdSphere(p, vec3(-0.4, 0.0, 0.0), 0.5),
        sdSphere(p, vec3( 0.4, 0.0, 0.0), 0.5),
        uSoftness);
}

void main() {
    vec2 uv = vUV * 2.0 - 1.0;
    vec3 ro = vec3(0.0, 0.0, -uDistance);
    vec3 rd = normalize(vec3(uv, 1.5));
    float t = 0.0;
    float d = 1.0;
    for (int i = 0; i < 64; ++i) {
        vec3 p = ro + rd * t;
        d = map(p);
        if (d < 0.001 || t > 20.0) break;
        t += d;
    }
    if (d < 0.01) {
        float shade = 1.0 - t * 0.1;
        fragColor = vec4(vec3(shade), 1.0);
    } else {
        fragColor = vec4(0.05, 0.05, 0.08, 1.0);
    }
}
