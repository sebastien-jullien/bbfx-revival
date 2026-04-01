#version 330 core
uniform sampler2D tex0;
uniform float time;
uniform float intensity;
uniform float block_size;
uniform float displacement;
in vec2 oUv0;
out vec4 fragColor;
float rand(vec2 co) { return fract(sin(dot(co, vec2(12.9898, 78.233))) * 43758.5453); }
void main() {
    float bs = max(block_size, 4.0) / 512.0;
    vec2 block = floor(oUv0 / bs) * bs;
    float r = rand(block + floor(time * 2.0));
    vec2 offset = vec2(0.0);
    if (r < intensity) {
        offset = (vec2(rand(block + 0.1), rand(block + 0.2)) - 0.5) * displacement;
    }
    vec4 col = texture(tex0, oUv0 + offset);
    // Color channel shift for glitch feel
    if (r < intensity * 0.5) {
        col.r = texture(tex0, oUv0 + offset + vec2(0.005, 0.0)).r;
    }
    fragColor = col;
}
