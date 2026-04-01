#version 330 core

uniform sampler2D tex0;
uniform float sampleDistance;
in vec2 oUv0;
out vec4 fragColor;

void main()
{
    vec2 samples[8] = vec2[](
        vec2(-1.0, -1.0), vec2( 0.0, -1.0), vec2( 1.0, -1.0),
        vec2(-1.0,  0.0), vec2( 1.0,  0.0),
        vec2(-1.0,  1.0), vec2( 0.0,  1.0), vec2( 1.0,  1.0)
    );

    vec4 col = texture(tex0, oUv0);
    for (int i = 0; i < 8; i++)
        col += texture(tex0, oUv0 + sampleDistance * samples[i] * 0.01);
    fragColor = col / 9.0;
}
