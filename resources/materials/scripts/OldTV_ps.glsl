#version 330 core

uniform sampler2D tex0;    // scene
uniform sampler3D tex1;    // random noise 3D
uniform sampler3D tex2;    // noise volume
uniform float distortionFreq;
uniform float distortionScale;
uniform float distortionRoll;
uniform float interference;
uniform float frameLimit;
uniform float frameShape;
uniform float frameSharpness;
uniform float time_0_X;
uniform float sin_time_0_X;
in vec2 oUv0;
out vec4 fragColor;

void main()
{
    // Scanline jitter
    float jitter = texture(tex2, vec3(oUv0, time_0_X * 0.1)).r;
    vec2 uv = oUv0;
    uv.x += (jitter - 0.5) * 0.01 * distortionScale;

    // Vertical roll
    uv.y += sin(uv.y * distortionFreq + time_0_X * distortionRoll) * 0.01;

    vec4 col = texture(tex0, uv);

    // Noise
    vec3 noise = texture(tex1, vec3(oUv0 * 6.0, sin_time_0_X * 0.5)).rgb;
    col.rgb += (noise - vec3(0.5)) * interference;

    // Vignette / frame shape
    vec2 edge = abs(oUv0 - vec2(0.5)) * 2.0;
    float frame = clamp(1.0 - pow(edge.x, frameSharpness) - pow(edge.y, frameSharpness), frameLimit, 1.0);
    col.rgb *= frame;

    fragColor = col;
}
