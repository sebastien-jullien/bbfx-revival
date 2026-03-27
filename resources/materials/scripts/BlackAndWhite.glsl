uniform sampler2D tex0;
varying vec2 oUv0;

void main()
{
    vec4 col = texture2D(tex0, oUv0);
    float grey = dot(col.rgb, vec3(0.299, 0.587, 0.114));
    gl_FragColor = vec4(grey, grey, grey, col.a);
}
