#version 330 core
uniform sampler2D tex0;
in vec2 oUv0;
out vec4 fragColor;
void main() {
    vec2 ts = 1.0 / vec2(textureSize(tex0, 0));
    vec3 tl = texture(tex0, oUv0 + vec2(-ts.x, -ts.y)).rgb;
    vec3 tm = texture(tex0, oUv0 + vec2(0.0, -ts.y)).rgb;
    vec3 tr = texture(tex0, oUv0 + vec2(ts.x, -ts.y)).rgb;
    vec3 ml = texture(tex0, oUv0 + vec2(-ts.x, 0.0)).rgb;
    vec3 mr = texture(tex0, oUv0 + vec2(ts.x, 0.0)).rgb;
    vec3 bl = texture(tex0, oUv0 + vec2(-ts.x, ts.y)).rgb;
    vec3 bm = texture(tex0, oUv0 + vec2(0.0, ts.y)).rgb;
    vec3 br = texture(tex0, oUv0 + vec2(ts.x, ts.y)).rgb;
    vec3 gx = -tl - 2.0*ml - bl + tr + 2.0*mr + br;
    vec3 gy = -tl - 2.0*tm - tr + bl + 2.0*bm + br;
    float edge = length(gx) + length(gy);
    fragColor = vec4(vec3(edge), 1.0);
}
