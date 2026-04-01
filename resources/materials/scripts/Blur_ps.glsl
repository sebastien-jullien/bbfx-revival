#version 330 core

uniform sampler2D tex0;
in vec2 oUv0;
in vec2 oUv1;
in vec2 oUv2;
in vec2 oUv3;
out vec4 fragColor;

void main()
{
    fragColor = 0.15 * texture(tex0, oUv0) +
                0.35 * texture(tex0, oUv1) +
                0.35 * texture(tex0, oUv2) +
                0.15 * texture(tex0, oUv3);
}
