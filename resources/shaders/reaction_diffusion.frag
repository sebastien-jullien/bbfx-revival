#version 330 core
uniform float time;
uniform float feed_rate;
uniform float kill_rate;
in vec2 oUv0;
out vec4 fragColor;
float hash(vec2 p) { return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453); }
void main() {
    // Simplified Gray-Scott visualization (no feedback texture — purely procedural)
    vec2 uv = oUv0 * 20.0;
    float n = 0.0;
    for (int i = 0; i < 5; i++) {
        float fi = float(i);
        vec2 offset = vec2(sin(time * 0.3 + fi), cos(time * 0.2 + fi * 1.7)) * 3.0;
        float d = length(fract(uv + offset) - 0.5);
        n += smoothstep(feed_rate * 5.0, kill_rate * 2.0, d);
    }
    n = fract(n * 0.5 + time * 0.05);
    vec3 col = 0.5 + 0.5 * cos(6.283 * (n + vec3(0.0, 0.33, 0.67)));
    fragColor = vec4(col, 1.0);
}
