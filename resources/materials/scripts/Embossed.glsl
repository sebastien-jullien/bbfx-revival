uniform sampler2D tex0;
uniform vec2 texelSize;
varying vec2 oUv0;

void main()
{
    vec4 c0 = texture2D(tex0, oUv0 + vec2(-texelSize.x, -texelSize.y));
    vec4 c1 = texture2D(tex0, oUv0);
    vec4 c2 = texture2D(tex0, oUv0 + vec2( texelSize.x,  texelSize.y));
    gl_FragColor = vec4((c2 - c0).rgb + vec3(0.5), 1.0);
}
