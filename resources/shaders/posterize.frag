#version 330 core
uniform sampler2D tex0;
uniform float levels;
in vec2 oUv0;
out vec4 fragColor;
void main() {
    vec4 col = texture(tex0, oUv0);
    float l = max(levels, 2.0);
    col.rgb = floor(col.rgb * l + 0.5) / l;
    fragColor = col;
}
