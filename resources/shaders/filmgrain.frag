#version 330 core
uniform sampler2D tex0;
uniform float time;
uniform float intensity;
in vec2 oUv0;
out vec4 fragColor;
float rand(vec2 co) { return fract(sin(dot(co, vec2(12.9898, 78.233))) * 43758.5453); }
void main() {
    vec4 col = texture(tex0, oUv0);
    float grain = (rand(oUv0 + time) - 0.5) * intensity;
    col.rgb += vec3(grain);
    fragColor = col;
}
