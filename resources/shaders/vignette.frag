#version 330 core
uniform sampler2D tex0;
uniform float strength;
in vec2 oUv0;
out vec4 fragColor;
void main() {
    vec4 col = texture(tex0, oUv0);
    float d = length(oUv0 - 0.5) * 1.414;
    col.rgb *= 1.0 - strength * d * d;
    fragColor = col;
}
