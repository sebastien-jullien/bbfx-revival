#version 330 core

// Edge Blend — gamma-corrected edge fade for multi-projector overlap (v3.4)
// Each blend_* value is the fraction [0, 0.5] of the output used for fading.
// When all values are 0: passthrough (no change).

uniform sampler2D tex0;

uniform float blend_left;
uniform float blend_right;
uniform float blend_top;
uniform float blend_bottom;
uniform float blend_gamma;

in  vec2 oUv0;
out vec4 fragColor;

void main() {
    vec4 color = texture(tex0, oUv0);
    float u = oUv0.x;
    float v = oUv0.y;
    float factor = 1.0;

    if (blend_left > 1e-5 && u < blend_left) {
        factor *= pow(clamp(u / blend_left, 0.0, 1.0), blend_gamma);
    }
    if (blend_right > 1e-5 && u > 1.0 - blend_right) {
        factor *= pow(clamp((1.0 - u) / blend_right, 0.0, 1.0), blend_gamma);
    }
    if (blend_top > 1e-5 && v < blend_top) {
        factor *= pow(clamp(v / blend_top, 0.0, 1.0), blend_gamma);
    }
    if (blend_bottom > 1e-5 && v > 1.0 - blend_bottom) {
        factor *= pow(clamp((1.0 - v) / blend_bottom, 0.0, 1.0), blend_gamma);
    }

    fragColor = vec4(color.rgb * factor, color.a);
}
