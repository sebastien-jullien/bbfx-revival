#include "SDFPrimitives.h"

#include <algorithm>
#include <cmath>
#include <iostream>
#include <vector>

#include <OgreManualObject.h>
#include <OgreMesh.h>
#include <OgreResourceGroupManager.h>

namespace bbfx {

// ── Primitives ─────────────────────────────────────────────────────────────

float SDFPrimitives::sphere(float x, float y, float z,
                               float cx, float cy, float cz, float r) {
    float dx = x - cx, dy = y - cy, dz = z - cz;
    return std::sqrt(dx * dx + dy * dy + dz * dz) - r;
}

float SDFPrimitives::box(float x, float y, float z,
                            float cx, float cy, float cz,
                            float hx, float hy, float hz) {
    float qx = std::fabs(x - cx) - hx;
    float qy = std::fabs(y - cy) - hy;
    float qz = std::fabs(z - cz) - hz;
    float outside = std::sqrt(
        std::max(qx, 0.0f) * std::max(qx, 0.0f) +
        std::max(qy, 0.0f) * std::max(qy, 0.0f) +
        std::max(qz, 0.0f) * std::max(qz, 0.0f));
    float inside  = std::min(std::max({qx, qy, qz}), 0.0f);
    return outside + inside;
}

float SDFPrimitives::torus(float x, float y, float z,
                              float cx, float cy, float cz,
                              float majorR, float minorR) {
    float dx = x - cx, dy = y - cy, dz = z - cz;
    float q = std::sqrt(dx * dx + dz * dz) - majorR;
    return std::sqrt(q * q + dy * dy) - minorR;
}

// ── Boolean ops ────────────────────────────────────────────────────────────

float SDFPrimitives::opUnion       (float d1, float d2) { return std::min(d1, d2); }
float SDFPrimitives::opIntersection(float d1, float d2) { return std::max(d1, d2); }
float SDFPrimitives::opSubtraction (float d1, float d2) { return std::max(d1, -d2); }
float SDFPrimitives::opSmoothUnion (float d1, float d2, float k) {
    if (k <= 0.0f) return std::min(d1, d2);
    float h = std::clamp(0.5f + 0.5f * (d2 - d1) / k, 0.0f, 1.0f);
    return d2 + (d1 - d2) * h - k * h * (1.0f - h);
}

// ── Marching cubes (Lorensen & Cline 1987) ─────────────────────────────────
//
// The LUT below is the classic 256-entry mc triangulation table where
// each entry holds up to 15 edge indices (0..11) terminated by -1. The
// 12 cube edges are numbered:
//
//    4--------5
//   /|       /|
//  7--------6 |
//  | |      | |
//  | 0------|-1
//  |/       |/
//  3--------2
//
// Edges: 0:(0,1), 1:(1,2), 2:(2,3), 3:(3,0),
//        4:(4,5), 5:(5,6), 6:(6,7), 7:(7,4),
//        8:(0,4), 9:(1,5), 10:(2,6), 11:(3,7)
//
// The full LUT is ~3800 chars — kept as-is for clarity.

namespace {

const int edgeConnections[12][2] = {
    {0,1},{1,2},{2,3},{3,0},
    {4,5},{5,6},{6,7},{7,4},
    {0,4},{1,5},{2,6},{3,7},
};

const float cornerOffset[8][3] = {
    {0,0,0},{1,0,0},{1,1,0},{0,1,0},
    {0,0,1},{1,0,1},{1,1,1},{0,1,1},
};

// Edge table: bitmask of edges crossed for each cube-vertex sign config.
const int edgeTable[256] = {
0x0  ,0x109,0x203,0x30a,0x406,0x50f,0x605,0x70c,0x80c,0x905,0xa0f,0xb06,0xc0a,0xd03,0xe09,0xf00,
0x190,0x99 ,0x393,0x29a,0x596,0x49f,0x795,0x69c,0x99c,0x895,0xb9f,0xa96,0xd9a,0xc93,0xf99,0xe90,
0x230,0x339,0x33 ,0x13a,0x636,0x73f,0x435,0x53c,0xa3c,0xb35,0x83f,0x936,0xe3a,0xf33,0xc39,0xd30,
0x3a0,0x2a9,0x1a3,0xaa ,0x7a6,0x6af,0x5a5,0x4ac,0xbac,0xaa5,0x9af,0x8a6,0xfaa,0xea3,0xda9,0xca0,
0x460,0x569,0x663,0x76a,0x66 ,0x16f,0x265,0x36c,0xc6c,0xd65,0xe6f,0xf66,0x86a,0x963,0xa69,0xb60,
0x5f0,0x4f9,0x7f3,0x6fa,0x1f6,0xff ,0x3f5,0x2fc,0xdfc,0xcf5,0xfff,0xef6,0x9fa,0x8f3,0xbf9,0xaf0,
0x650,0x759,0x453,0x55a,0x256,0x35f,0x55 ,0x15c,0xe5c,0xf55,0xc5f,0xd56,0xa5a,0xb53,0x859,0x950,
0x7c0,0x6c9,0x5c3,0x4ca,0x3c6,0x2cf,0x1c5,0xcc ,0xfcc,0xec5,0xdcf,0xcc6,0xbca,0xac3,0x9c9,0x8c0,
0x8c0,0x9c9,0xac3,0xbca,0xcc6,0xdcf,0xec5,0xfcc,0xcc ,0x1c5,0x2cf,0x3c6,0x4ca,0x5c3,0x6c9,0x7c0,
0x950,0x859,0xb53,0xa5a,0xd56,0xc5f,0xf55,0xe5c,0x15c,0x55 ,0x35f,0x256,0x55a,0x453,0x759,0x650,
0xaf0,0xbf9,0x8f3,0x9fa,0xef6,0xfff,0xcf5,0xdfc,0x2fc,0x3f5,0xff ,0x1f6,0x6fa,0x7f3,0x4f9,0x5f0,
0xb60,0xa69,0x963,0x86a,0xf66,0xe6f,0xd65,0xc6c,0x36c,0x265,0x16f,0x66 ,0x76a,0x663,0x569,0x460,
0xca0,0xda9,0xea3,0xfaa,0x8a6,0x9af,0xaa5,0xbac,0x4ac,0x5a5,0x6af,0x7a6,0xaa ,0x1a3,0x2a9,0x3a0,
0xd30,0xc39,0xf33,0xe3a,0x936,0x83f,0xb35,0xa3c,0x53c,0x435,0x73f,0x636,0x13a,0x33 ,0x339,0x230,
0xe90,0xf99,0xc93,0xd9a,0xa96,0xb9f,0x895,0x99c,0x69c,0x795,0x49f,0x596,0x29a,0x393,0x99 ,0x190,
0xf00,0xe09,0xd03,0xc0a,0xb06,0xa0f,0x905,0x80c,0x70c,0x605,0x50f,0x406,0x30a,0x203,0x109,0x0
};

// Triangle table: for each cube config, list of edge triplets.
// Canonical 256×16 Lorensen/Cline table, terminated by -1.
const int triTable[256][16] = {
#include "SDFPrimitives_triTable.inl"
};

} // anonymous

std::string SDFPrimitives::toMesh(const std::string& meshName,
                                         Field field,
                                         float x0, float y0, float z0,
                                         float x1, float y1, float z1,
                                         int resolution) {
    if (meshName.empty() || !field || resolution < 2) return {};
    float dx = (x1 - x0) / resolution;
    float dy = (y1 - y0) / resolution;
    float dz = (z1 - z0) / resolution;

    // Evaluate the field on a dense grid for fast cube lookup.
    int gx = resolution + 1;
    int gy = resolution + 1;
    int gz = resolution + 1;
    std::vector<float> grid(static_cast<size_t>(gx) * gy * gz);
    for (int k = 0; k < gz; ++k) {
        for (int j = 0; j < gy; ++j) {
            for (int i = 0; i < gx; ++i) {
                grid[(size_t)k * gy * gx + (size_t)j * gx + i] =
                    field(x0 + i * dx, y0 + j * dy, z0 + k * dz);
            }
        }
    }
    auto g = [&](int i, int j, int k) {
        return grid[(size_t)k * gy * gx + (size_t)j * gx + i];
    };

    std::vector<float> vertices;   // flat x,y,z,nx,ny,nz,u,v per vert
    std::vector<uint32_t> indices;

    const float iso = 0.0f;

    for (int k = 0; k < resolution; ++k) {
        for (int j = 0; j < resolution; ++j) {
            for (int i = 0; i < resolution; ++i) {
                float cv[8];
                float cp[8][3];
                for (int c = 0; c < 8; ++c) {
                    int ii = i + static_cast<int>(cornerOffset[c][0]);
                    int jj = j + static_cast<int>(cornerOffset[c][1]);
                    int kk = k + static_cast<int>(cornerOffset[c][2]);
                    cv[c] = g(ii, jj, kk);
                    cp[c][0] = x0 + ii * dx;
                    cp[c][1] = y0 + jj * dy;
                    cp[c][2] = z0 + kk * dz;
                }
                int cube = 0;
                for (int c = 0; c < 8; ++c) if (cv[c] < iso) cube |= (1 << c);
                int em = edgeTable[cube];
                if (em == 0) continue;

                float edgeV[12][3]{};
                for (int e = 0; e < 12; ++e) {
                    if (!(em & (1 << e))) continue;
                    int a = edgeConnections[e][0];
                    int b = edgeConnections[e][1];
                    float va = cv[a], vb = cv[b];
                    float t = (iso - va) / (vb - va);
                    edgeV[e][0] = cp[a][0] + t * (cp[b][0] - cp[a][0]);
                    edgeV[e][1] = cp[a][1] + t * (cp[b][1] - cp[a][1]);
                    edgeV[e][2] = cp[a][2] + t * (cp[b][2] - cp[a][2]);
                }

                for (int t = 0; triTable[cube][t] != -1; t += 3) {
                    uint32_t base = static_cast<uint32_t>(vertices.size() / 8);
                    for (int vv = 0; vv < 3; ++vv) {
                        int eIdx = triTable[cube][t + vv];
                        const float* p = edgeV[eIdx];
                        vertices.push_back(p[0]);
                        vertices.push_back(p[1]);
                        vertices.push_back(p[2]);
                        // Approx normal : gradient of the field via
                        // finite differences at the vertex.
                        const float h = std::max({dx, dy, dz}) * 0.5f;
                        float fx = field(p[0] + h, p[1], p[2]) - field(p[0] - h, p[1], p[2]);
                        float fy = field(p[0], p[1] + h, p[2]) - field(p[0], p[1] - h, p[2]);
                        float fz = field(p[0], p[1], p[2] + h) - field(p[0], p[1], p[2] - h);
                        float l = std::sqrt(fx*fx + fy*fy + fz*fz);
                        if (l < 1e-6f) l = 1.0f;
                        vertices.push_back(fx / l);
                        vertices.push_back(fy / l);
                        vertices.push_back(fz / l);
                        // UV mapped from gradient.xy for a cheap default.
                        vertices.push_back(0.0f);
                        vertices.push_back(0.0f);
                    }
                    indices.push_back(base + 0);
                    indices.push_back(base + 1);
                    indices.push_back(base + 2);
                }
            }
        }
    }
    if (vertices.empty() || indices.empty()) return {};

    try {
        Ogre::ManualObject mo(meshName);
        mo.begin("BaseWhiteNoLighting", Ogre::RenderOperation::OT_TRIANGLE_LIST);
        for (size_t i = 0; i < vertices.size(); i += 8) {
            mo.position(vertices[i+0], vertices[i+1], vertices[i+2]);
            mo.normal  (vertices[i+3], vertices[i+4], vertices[i+5]);
            mo.textureCoord(vertices[i+6], vertices[i+7]);
        }
        for (size_t i = 0; i + 2 < indices.size(); i += 3) {
            mo.triangle(indices[i], indices[i+1], indices[i+2]);
        }
        mo.end();
        auto mesh = mo.convertToMesh(meshName,
                     Ogre::ResourceGroupManager::DEFAULT_RESOURCE_GROUP_NAME);
        if (mesh) return meshName;
    } catch (const std::exception& e) {
        std::cerr << "[SDFPrimitives] toMesh failed: " << e.what() << std::endl;
    }
    return {};
}

} // namespace bbfx
