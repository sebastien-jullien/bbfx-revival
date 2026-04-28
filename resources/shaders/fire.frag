#version 330 core
// Procedural fire — gradient noise + fire palette
uniform float intensity;  // def 1.0
uniform float speed;      // def 1.0
uniform float detail;     // def 3.0
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

float fbm(vec2 p) {
    float f = 0.0;
    float amp = 0.5;
    for (int i = 0; i < 5; i++) {
        f += amp * noise(p);
        p *= 2.0;
        amp *= 0.5;
    }
    return f;
}

void main() {
    vec2 uv = oUv0;
    uv.y = 1.0 - uv.y; // flip so fire rises
    float t = time * speed;
    float n = fbm(uv * detail + vec2(0, -t * 2.0));
    n += fbm(uv * detail * 2.0 + vec2(t * 0.5, -t * 3.0)) * 0.5;
    float fire = n * intensity * (1.0 - uv.y); // fade towards top
    // Fire color palette
    vec3 col = vec3(1.0, 0.0, 0.0);
    if (fire > 0.3) col = vec3(1.0, 0.5, 0.0);
    if (fire > 0.5) col = vec3(1.0, 0.8, 0.2);
    if (fire > 0.7) col = vec3(1.0, 1.0, 0.6);
    col *= fire;
    fragColor = vec4(col, 1.0);
}
