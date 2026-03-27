// DOF blend fragment shader — mix blurred and sharp based on distance from focus
uniform sampler2D tex0; // blurred
uniform sampler2D tex1; // sharp
uniform float focus;
uniform float range;
varying vec2 oUv0;

void main()
{
    vec4 sharp = texture2D(tex1, oUv0);
    vec4 blurred = texture2D(tex0, oUv0);
    float dist = abs(oUv0.y - focus);
    float blend = clamp(dist / range, 0.0, 1.0);
    gl_FragColor = mix(sharp, blurred, blend);
}
