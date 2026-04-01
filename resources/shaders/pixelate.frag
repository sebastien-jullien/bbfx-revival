#version 330 core
uniform sampler2D tex0;
uniform float pixel_size;
in vec2 oUv0;
out vec4 fragColor;
void main() {
    float ps = max(pixel_size, 1.0) / 512.0;
    vec2 uv = floor(oUv0 / ps) * ps + ps * 0.5;
    fragColor = texture(tex0, uv);
}
