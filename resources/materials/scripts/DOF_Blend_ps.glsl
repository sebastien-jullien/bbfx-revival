#version 330 core

uniform sampler2D tex0; // blurred
uniform sampler2D tex1; // sharp
uniform float focus;
uniform float range;
in vec2 oUv0;
out vec4 fragColor;

void main()
{
    vec4 sharp = texture(tex1, oUv0);
    vec4 blurred = texture(tex0, oUv0);
    float dist = abs(oUv0.y - focus);
    float blend = clamp(dist / range, 0.0, 1.0);
    fragColor = mix(sharp, blurred, blend);
}
