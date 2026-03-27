// Horizontal blur vertex shader — offsets UVs horizontally
uniform float size;
varying vec2 oUv0;
varying vec2 oUv1;
varying vec2 oUv2;
varying vec2 oUv3;

void main()
{
    gl_Position = ftransform();
    vec2 uv = gl_MultiTexCoord0.xy;
    float texelSize = 1.0 / 128.0;
    oUv0 = uv + vec2(-texelSize, 0.0);
    oUv1 = uv;
    oUv2 = uv + vec2( texelSize, 0.0);
    oUv3 = uv + vec2( texelSize * 2.0, 0.0);
}
