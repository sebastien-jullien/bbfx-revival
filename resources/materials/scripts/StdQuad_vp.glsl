varying vec2 oUv0;

void main()
{
    gl_Position = ftransform();
    oUv0 = gl_MultiTexCoord0.xy;
}
