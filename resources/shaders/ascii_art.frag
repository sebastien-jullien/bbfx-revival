#version 330 core
uniform sampler2D tex0;
uniform float char_size;
uniform float brightness;
in vec2 oUv0;
out vec4 fragColor;
void main() {
    float cs = max(char_size, 4.0);
    vec2 cell = floor(oUv0 * vec2(textureSize(tex0, 0)) / cs) * cs / vec2(textureSize(tex0, 0));
    vec4 col = texture(tex0, cell);
    float lum = dot(col.rgb, vec3(0.299, 0.587, 0.114)) * brightness;
    // Simple ASCII density visualization using brightness bands
    float density = floor(lum * 8.0) / 8.0;
    // Create a dot pattern based on density
    vec2 cellFrac = fract(oUv0 * vec2(textureSize(tex0, 0)) / cs);
    float dot_size = density;
    float d = length(cellFrac - 0.5);
    float c = step(d, dot_size * 0.5);
    fragColor = vec4(vec3(c) * col.rgb, 1.0);
}
