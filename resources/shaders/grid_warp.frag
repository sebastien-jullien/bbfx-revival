#version 330 core

// Grid Warp — bilinear mesh deformation over a 4x4 control-point grid (v3.4 Lot K)
//
// grid[i] packs two control points per vec4:
//   grid[i].xy = source UV for control point (i*2)
//   grid[i].zw = source UV for control point (i*2+1)
// Points are stored row-major: point (row, col) = index row*4 + col.
// Identity: point (r,c) = (c/3, r/3) for a 4x4 grid.

uniform sampler2D tex0;
uniform vec4 grid[8]; // 16 vec2 packed into 8 vec4

in  vec2 oUv0;
out vec4 fragColor;

const int N = 4; // grid dimension (NxN control points, (N-1)x(N-1) cells)

// Unpack control point index idx from the packed grid array
vec2 getGridPoint(int idx) {
    int i   = idx / 2;
    bool lo = (idx % 2 == 0);
    return lo ? grid[i].xy : grid[i].zw;
}

void main() {
    vec2 p = oUv0;
    float invN1 = 1.0 / float(N - 1);

    // Find cell (ci, cj) — clamp to last cell at the boundary
    int ci = clamp(int(p.x / invN1), 0, N - 2);
    int cj = clamp(int(p.y / invN1), 0, N - 2);

    // Local coords within the cell [0, 1]
    float s = clamp((p.x - float(ci) * invN1) / invN1, 0.0, 1.0);
    float t = clamp((p.y - float(cj) * invN1) / invN1, 0.0, 1.0);

    // Bilinear interpolation of the 4 cell corners (row-major: row=cj, col=ci)
    vec2 tl = getGridPoint( cj      * N + ci);
    vec2 tr = getGridPoint( cj      * N + ci + 1);
    vec2 bl = getGridPoint((cj + 1) * N + ci);
    vec2 br = getGridPoint((cj + 1) * N + ci + 1);

    vec2 uv = mix(mix(tl, tr, s), mix(bl, br, s), t);

    fragColor = texture(tex0, clamp(uv, 0.0, 1.0));
}
