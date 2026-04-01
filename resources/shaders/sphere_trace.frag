#version 330 core
uniform float time;
uniform float morph;
uniform float rotation_speed;
in vec2 oUv0;
out vec4 fragColor;
float sdSphere(vec3 p, float r) { return length(p) - r; }
float sdBox(vec3 p, vec3 b) { vec3 d = abs(p) - b; return length(max(d, 0.0)) + min(max(d.x, max(d.y, d.z)), 0.0); }
float sdTorus(vec3 p, vec2 t) { vec2 q = vec2(length(p.xz) - t.x, p.y); return length(q) - t.y; }
mat3 rotY(float a) { float c = cos(a), s = sin(a); return mat3(c, 0, s, 0, 1, 0, -s, 0, c); }
void main() {
    vec2 uv = oUv0 * 2.0 - 1.0;
    vec3 ro = vec3(0.0, 0.0, 3.0);
    vec3 rd = normalize(vec3(uv, -1.5));
    float t = 0.0;
    vec3 col = vec3(0.0);
    for (int i = 0; i < 64; i++) {
        vec3 p = ro + rd * t;
        p = rotY(time * rotation_speed) * p;
        float d1 = sdSphere(p, 0.8);
        float d2 = sdBox(p, vec3(0.6));
        float d3 = sdTorus(p, vec2(0.7, 0.25));
        float d = mix(d1, mix(d2, d3, clamp(morph * 2.0 - 1.0, 0.0, 1.0)), clamp(morph, 0.0, 1.0));
        if (d < 0.001) {
            vec3 n = normalize(vec3(d, d, d)); // simplified normal
            col = vec3(0.5) + 0.5 * cos(6.283 * (vec3(0.0, 0.33, 0.67) + t * 0.1));
            col *= max(dot(n, normalize(vec3(1.0, 1.0, 1.0))), 0.2);
            break;
        }
        t += d;
        if (t > 10.0) break;
    }
    fragColor = vec4(col, 1.0);
}
