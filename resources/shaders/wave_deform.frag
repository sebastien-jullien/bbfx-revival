#version 330 core
uniform sampler2D tex0;
uniform float time;
uniform float amplitude;
uniform float frequency;
uniform float speed;
in vec2 oUv0;
out vec4 fragColor;
void main() {
    vec2 uv = oUv0;
    float dist = length(uv - 0.5);
    uv += amplitude * 0.01 * sin(dist * frequency * 20.0 - time * speed * 5.0) * normalize(uv - 0.5);
    fragColor = texture(tex0, uv);
}
