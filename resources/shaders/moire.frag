#version 330 core
// Moire interference pattern
uniform float frequency1;  // def 30.0
uniform float frequency2;  // def 31.0
uniform float rotation;    // def 0.0 (degrees)
uniform float time;
uniform sampler2D tex0;
in vec2 oUv0;
out vec4 fragColor;

void main() {
    vec2 uv = oUv0 - 0.5;
    float rad = rotation * 3.14159 / 180.0 + time * 0.2;
    float c = cos(rad), s = sin(rad);
    vec2 uv2 = vec2(uv.x * c - uv.y * s, uv.x * s + uv.y * c);
    float g1 = sin(uv.x * frequency1 + time) * sin(uv.y * frequency1);
    float g2 = sin(uv2.x * frequency2) * sin(uv2.y * frequency2 + time * 0.7);
    float moire = (g1 + g2) * 0.5 + 0.5;
    vec3 col = 0.5 + 0.5 * cos(6.28318 * (moire + vec3(0.0, 0.33, 0.67)));
    fragColor = vec4(col, 1.0);
}
