// Glass distortion fragment shader
uniform sampler2D tex0; // scene
uniform sampler2D tex1; // normal map for distortion
varying vec2 oUv0;

void main()
{
    vec2 normal = texture2D(tex1, oUv0 * 2.5).rg * 2.0 - 1.0;
    vec2 uv = oUv0 + normal * 0.05;
    gl_FragColor = texture2D(tex0, uv);
}
