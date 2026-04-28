#version 330 core
uniform sampler2D tex0;
in vec2 oUv0;
out vec4 fragColor;

void main() {
    vec2 texelSize = 1.0 / vec2(textureSize(tex0, 0));
    vec4 c0 = texture(tex0, oUv0 + vec2(-texelSize.x, -texelSize.y));
    vec4 c1 = texture(tex0, oUv0);
    vec4 c2 = texture(tex0, oUv0 + vec2( texelSize.x,  texelSize.y));
    fragColor = vec4((c2 - c0).rgb + vec3(0.5), 1.0);
}
