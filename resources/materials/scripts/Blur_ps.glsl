// Simple 4-tap blur fragment shader
uniform sampler2D tex0;
varying vec2 oUv0;
varying vec2 oUv1;
varying vec2 oUv2;
varying vec2 oUv3;

void main()
{
    gl_FragColor = 0.15 * texture2D(tex0, oUv0) +
                   0.35 * texture2D(tex0, oUv1) +
                   0.35 * texture2D(tex0, oUv2) +
                   0.15 * texture2D(tex0, oUv3);
}
