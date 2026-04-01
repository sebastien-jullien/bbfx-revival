#version 330 core
uniform sampler2D tex0;
uniform float intensity;
uniform float falloff;
in vec2 oUv0;
out vec4 fragColor;
void main() {
    vec2 dir = oUv0 - 0.5;
    float d = length(dir) * falloff;
    vec2 offset = normalize(dir) * intensity * d * 0.001;
    float r = texture(tex0, oUv0 + offset).r;
    float g = texture(tex0, oUv0).g;
    float b = texture(tex0, oUv0 - offset).b;
    fragColor = vec4(r, g, b, 1.0);
}
