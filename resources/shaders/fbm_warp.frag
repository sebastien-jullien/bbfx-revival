#version 330 core
// FBM noise with domain warping
uniform float scale;          // def 3.0
uniform float speed;          // def 0.5
uniform float warp_strength;  // def 1.0
uniform float octaves;        // def 4.0
uniform float time;
uniform sampler2D tex0;
in vec2 oUv0;
out vec4 fragColor;

float hash(vec2 p) {
    return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453);
}

float noise(vec2 p) {
    vec2 i = floor(p);
    vec2 f = fract(p);
    f = f * f * (3.0 - 2.0 * f);
    float a = hash(i);
    float b = hash(i + vec2(1, 0));
    float c = hash(i + vec2(0, 1));
    float d = hash(i + vec2(1, 1));
    return mix(mix(a, b, f.x), mix(c, d, f.x), f.y);
}

float fbm(vec2 p, int oct) {
    float f = 0.0, amp = 0.5;
    for (int i = 0; i < oct; i++) {
        f += amp * noise(p);
        p *= 2.0;
        amp *= 0.5;
    }
    return f;
}

void main() {
    vec2 p = oUv0 * scale;
    float t = time * speed;
    // Domain warping
    vec2 q = vec2(fbm(p + t, int(octaves)), fbm(p + vec2(5.2, 1.3) + t, int(octaves)));
    vec2 r = vec2(fbm(p + warp_strength * q + vec2(1.7, 9.2) + t * 0.15, int(octaves)),
                  fbm(p + warp_strength * q + vec2(8.3, 2.8) + t * 0.126, int(octaves)));
    float f = fbm(p + warp_strength * r, int(octaves));
    vec3 col = mix(vec3(0.1, 0.2, 0.4), vec3(0.9, 0.6, 0.2), f);
    col = mix(col, vec3(0.2, 0.8, 0.5), length(q));
    col = mix(col, vec3(0.9, 0.3, 0.1), r.x);
    fragColor = vec4(col, 1.0);
}
