#version 330 core
// Hexagonal tiling pattern
uniform float scale;       // def 10.0
uniform float gap;         // def 0.05
uniform float time;
uniform float color_shift; // def 0.0
uniform sampler2D tex0;
in vec2 oUv0;
out vec4 fragColor;

vec2 hexCoord(vec2 p) {
    vec2 q = vec2(p.x * 2.0 / sqrt(3.0), p.y + p.x / sqrt(3.0));
    vec2 pi = floor(q);
    vec2 pf = fract(q);
    float v = mod(pi.x + pi.y, 3.0);
    return vec2(v, length(pf - 0.5));
}

void main() {
    vec2 p = oUv0 * scale;
    vec2 h = hexCoord(p);
    float edge = smoothstep(0.5 - gap, 0.5, h.y);
    float hue = fract(h.x / 3.0 + color_shift / 360.0 + time * 0.1);
    // HSV to RGB
    vec3 col = 0.5 + 0.5 * cos(6.28318 * (hue + vec3(0.0, 0.33, 0.67)));
    col *= (1.0 - edge);
    fragColor = vec4(col, 1.0);
}
