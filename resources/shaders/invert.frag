#version 330 core
uniform sampler2D tex0;
in vec2 oUv0;
out vec4 fragColor;
void main() {
    vec4 col = texture(tex0, oUv0);
    fragColor = vec4(1.0 - col.rgb, col.a);
}
