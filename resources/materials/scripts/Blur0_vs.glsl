// Vertical blur vertex shader — offsets UVs vertically
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
    oUv0 = uv + vec2(0.0, -texelSize);
    oUv1 = uv;
    oUv2 = uv + vec2(0.0,  texelSize);
    oUv3 = uv + vec2(0.0,  texelSize * 2.0);
}
