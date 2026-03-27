// DOF blur fragment shader
uniform sampler2D tex0;
uniform float sampleDistance;
varying vec2 oUv0;

void main()
{
    vec2 samples[8];
    samples[0] = vec2(-1.0, -1.0);
    samples[1] = vec2( 0.0, -1.0);
    samples[2] = vec2( 1.0, -1.0);
    samples[3] = vec2(-1.0,  0.0);
    samples[4] = vec2( 1.0,  0.0);
    samples[5] = vec2(-1.0,  1.0);
    samples[6] = vec2( 0.0,  1.0);
    samples[7] = vec2( 1.0,  1.0);

    vec4 col = texture2D(tex0, oUv0);
    for (int i = 0; i < 8; i++)
    {
        col += texture2D(tex0, oUv0 + sampleDistance * samples[i] * 0.01);
    }
    gl_FragColor = col / 9.0;
}
