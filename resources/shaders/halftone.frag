#version 330 core

uniform sampler2D tex0;
uniform float dot_size;
uniform float angle;

in  vec2 oUv0;
out vec4 fragColor;

void main() {
    // Rotation matrix for halftone angle
    float a = radians(angle);
    float ca = cos(a), sa = sin(a);

    // Scale UVs by dot density
    vec2 uv = oUv0;
    float scale = 1.0 / max(dot_size, 0.001);
    vec2 grid = vec2(uv.x * scale, uv.y * scale);

    // Rotate grid
    vec2 rotated = vec2(
        grid.x * ca - grid.y * sa,
        grid.x * sa + grid.y * ca
    );

    // Cell center
    vec2 cell = floor(rotated) + 0.5;

    // Unrotate to get sample position
    vec2 samplePos = vec2(
        cell.x * ca + cell.y * sa,
        -cell.x * sa + cell.y * ca
    ) / scale;

    // Sample color at cell center
    vec4 col = texture(tex0, clamp(samplePos, 0.0, 1.0));
    float lum = dot(col.rgb, vec3(0.299, 0.587, 0.114));

    // Distance from pixel to cell center (in rotated space)
    vec2 diff = fract(rotated) - 0.5;
    float dist = length(diff);

    // Dot radius proportional to luminance
    float radius = sqrt(lum) * 0.5;

    // CMYK-style: draw dot if within radius
    float aa = fwidth(dist);
    float dot = 1.0 - smoothstep(radius - aa, radius + aa, dist);

    fragColor = vec4(col.rgb * dot, 1.0);
}
