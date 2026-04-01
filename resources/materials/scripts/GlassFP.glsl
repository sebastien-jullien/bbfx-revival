#version 330 core

uniform sampler2D tex0; // scene
uniform sampler2D tex1; // normal map for distortion
in vec2 oUv0;
out vec4 fragColor;

void main()
{
    vec2 normal = texture(tex1, oUv0 * 2.5).rg * 2.0 - 1.0;
    vec2 uv = oUv0 + normal * 0.05;
    fragColor = texture(tex0, uv);
}
