#version 330 core
// Audio waveform visualizer
uniform float amplitude;   // def 0.5
uniform float frequency;   // def 4.0
uniform float thickness;   // def 0.02
uniform float time;
uniform sampler2D tex0;
in vec2 oUv0;
out vec4 fragColor;

void main() {
    vec2 uv = oUv0;
    float y = 0.5 + amplitude * sin(uv.x * frequency * 6.28318 + time * 3.0);
    y += amplitude * 0.3 * sin(uv.x * frequency * 2.0 * 6.28318 + time * 5.0);
    y += amplitude * 0.1 * sin(uv.x * frequency * 4.0 * 6.28318 + time * 7.0);
    float dist = abs(uv.y - y);
    float line = smoothstep(thickness, 0.0, dist);
    // Glow
    float glow = smoothstep(thickness * 10.0, 0.0, dist) * 0.3;
    vec3 col = vec3(0.2, 0.8, 0.3) * line + vec3(0.1, 0.4, 0.2) * glow;
    fragColor = vec4(col, 1.0);
}
