#version 330 core
uniform sampler2D tex0;
uniform float time;
uniform float tracking;
uniform float noise_amount;
uniform float color_bleed;
uniform float scanline;
in vec2 oUv0;
out vec4 fragColor;
float rand(vec2 co) { return fract(sin(dot(co, vec2(12.9898, 78.233))) * 43758.5453); }
void main() {
    vec2 uv = oUv0;
    uv.x += (rand(vec2(uv.y * 100.0, time)) - 0.5) * tracking * 0.01;
    uv.y += sin(time * 50.0 + uv.y * 100.0) * tracking * 0.002;
    vec4 col = texture(tex0, uv);
    col.r = texture(tex0, uv + vec2(color_bleed * 0.003, 0.0)).r;
    col.b = texture(tex0, uv - vec2(color_bleed * 0.003, 0.0)).b;
    float n = rand(uv + time) * noise_amount;
    col.rgb += vec3(n) - noise_amount * 0.5;
    float sl = sin(oUv0.y * 800.0) * 0.5 + 0.5;
    col.rgb *= 1.0 - scanline * 0.3 * sl;
    fragColor = col;
}
