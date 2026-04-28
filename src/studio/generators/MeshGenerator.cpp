#include "MeshGenerator.h"
#include <OgreMeshManager.h>
#include <OgreSubMesh.h>
#include <OgreHardwareBufferManager.h>
#include <cmath>
#include <cstring>
#include <vector>

namespace bbfx {

using namespace Ogre;

static int sMeshGenCounter = 0;

// Helper to create a mesh from vertex/index data
static MeshPtr createMeshFromData(const std::string& name,
    const std::vector<float>& positions,
    const std::vector<float>& normals,
    const std::vector<float>& uvs,
    const std::vector<uint32_t>& indices)
{
    auto mesh = MeshManager::getSingleton().createManual(name,
        ResourceGroupManager::DEFAULT_RESOURCE_GROUP_NAME);

    auto* sub = mesh->createSubMesh();
    size_t nVerts = positions.size() / 3;
    size_t nIdx = indices.size();

    // Vertex data
    auto* vd = new VertexData();
    sub->useSharedVertices = false;
    sub->vertexData = vd;
    vd->vertexCount = nVerts;

    size_t offset = 0;
    vd->vertexDeclaration->addElement(0, offset, VET_FLOAT3, VES_POSITION);
    offset += VertexElement::getTypeSize(VET_FLOAT3);
    vd->vertexDeclaration->addElement(0, offset, VET_FLOAT3, VES_NORMAL);
    offset += VertexElement::getTypeSize(VET_FLOAT3);
    vd->vertexDeclaration->addElement(0, offset, VET_FLOAT2, VES_TEXTURE_COORDINATES);
    offset += VertexElement::getTypeSize(VET_FLOAT2);

    auto vbuf = HardwareBufferManager::getSingleton().createVertexBuffer(
        offset, nVerts, HBU_GPU_ONLY);

    std::vector<float> interleaved(nVerts * 8);
    for (size_t i = 0; i < nVerts; i++) {
        interleaved[i*8+0] = positions[i*3+0];
        interleaved[i*8+1] = positions[i*3+1];
        interleaved[i*8+2] = positions[i*3+2];
        interleaved[i*8+3] = normals[i*3+0];
        interleaved[i*8+4] = normals[i*3+1];
        interleaved[i*8+5] = normals[i*3+2];
        interleaved[i*8+6] = (i < uvs.size()/2) ? uvs[i*2+0] : 0.0f;
        interleaved[i*8+7] = (i < uvs.size()/2) ? uvs[i*2+1] : 0.0f;
    }
    vbuf->writeData(0, interleaved.size() * sizeof(float), interleaved.data());
    vd->vertexBufferBinding->setBinding(0, vbuf);

    // Index data
    auto ibuf = HardwareBufferManager::getSingleton().createIndexBuffer(
        HardwareIndexBuffer::IT_32BIT, nIdx, HBU_GPU_ONLY);
    ibuf->writeData(0, nIdx * sizeof(uint32_t), indices.data());
    sub->indexData->indexBuffer = ibuf;
    sub->indexData->indexCount = nIdx;
    sub->indexData->indexStart = 0;

    // Bounds
    AxisAlignedBox aabb;
    for (size_t i = 0; i < nVerts; i++)
        aabb.merge(Vector3(positions[i*3], positions[i*3+1], positions[i*3+2]));
    mesh->_setBounds(aabb);
    mesh->_setBoundingSphereRadius(aabb.getHalfSize().length());
    mesh->load();

    return mesh;
}

MeshPtr MeshGenerator::generateSphere(int rings, int segments) {
    std::string name = "proc_sphere_" + std::to_string(++sMeshGenCounter);
    std::vector<float> pos, norm, uv;
    std::vector<uint32_t> idx;

    for (int y = 0; y <= rings; y++) {
        for (int x = 0; x <= segments; x++) {
            float u = (float)x / segments;
            float v = (float)y / rings;
            float theta = u * 2.0f * Math::PI;
            float phi = v * Math::PI;
            float px = std::sin(phi) * std::cos(theta);
            float py = std::cos(phi);
            float pz = std::sin(phi) * std::sin(theta);
            pos.push_back(px); pos.push_back(py); pos.push_back(pz);
            norm.push_back(px); norm.push_back(py); norm.push_back(pz);
            uv.push_back(u); uv.push_back(v);
        }
    }
    for (int y = 0; y < rings; y++) {
        for (int x = 0; x < segments; x++) {
            uint32_t a = y * (segments + 1) + x;
            uint32_t b = a + segments + 1;
            idx.push_back(a); idx.push_back(b); idx.push_back(a + 1);
            idx.push_back(b); idx.push_back(b + 1); idx.push_back(a + 1);
        }
    }
    return createMeshFromData(name, pos, norm, uv, idx);
}

MeshPtr MeshGenerator::generateGeosphere(int frequency) {
    // Simplified: use sphere with high ring/segment count proportional to frequency
    int res = std::max(frequency * 8, 16);
    return generateSphere(res, res * 2);
}

MeshPtr MeshGenerator::generateTorus(float majorRadius, float minorRadius, int rings, int segments) {
    std::string name = "proc_torus_" + std::to_string(++sMeshGenCounter);
    std::vector<float> pos, norm, uv;
    std::vector<uint32_t> idx;

    for (int i = 0; i <= rings; i++) {
        float u = (float)i / rings;
        float theta = u * 2.0f * Math::PI;
        for (int j = 0; j <= segments; j++) {
            float v = (float)j / segments;
            float phi = v * 2.0f * Math::PI;
            float cx = majorRadius * std::cos(theta);
            float cz = majorRadius * std::sin(theta);
            float px = (majorRadius + minorRadius * std::cos(phi)) * std::cos(theta);
            float py = minorRadius * std::sin(phi);
            float pz = (majorRadius + minorRadius * std::cos(phi)) * std::sin(theta);
            float nx = std::cos(phi) * std::cos(theta);
            float ny = std::sin(phi);
            float nz = std::cos(phi) * std::sin(theta);
            pos.push_back(px); pos.push_back(py); pos.push_back(pz);
            norm.push_back(nx); norm.push_back(ny); norm.push_back(nz);
            uv.push_back(u); uv.push_back(v);
        }
    }
    for (int i = 0; i < rings; i++) {
        for (int j = 0; j < segments; j++) {
            uint32_t a = i * (segments + 1) + j;
            uint32_t b = a + segments + 1;
            idx.push_back(a); idx.push_back(b); idx.push_back(a + 1);
            idx.push_back(b); idx.push_back(b + 1); idx.push_back(a + 1);
        }
    }
    return createMeshFromData(name, pos, norm, uv, idx);
}

MeshPtr MeshGenerator::generateCylinder(float radius, float height, int segments) {
    std::string name = "proc_cylinder_" + std::to_string(++sMeshGenCounter);
    std::vector<float> pos, norm, uv;
    std::vector<uint32_t> idx;
    float halfH = height * 0.5f;

    // Side vertices
    for (int i = 0; i <= segments; i++) {
        float u = (float)i / segments;
        float theta = u * 2.0f * Math::PI;
        float nx = std::cos(theta), nz = std::sin(theta);
        // Bottom
        pos.push_back(radius*nx); pos.push_back(-halfH); pos.push_back(radius*nz);
        norm.push_back(nx); norm.push_back(0); norm.push_back(nz);
        uv.push_back(u); uv.push_back(1);
        // Top
        pos.push_back(radius*nx); pos.push_back(halfH); pos.push_back(radius*nz);
        norm.push_back(nx); norm.push_back(0); norm.push_back(nz);
        uv.push_back(u); uv.push_back(0);
    }
    for (int i = 0; i < segments; i++) {
        uint32_t b = i * 2, t = b + 1;
        idx.push_back(b); idx.push_back(b+2); idx.push_back(t);
        idx.push_back(t); idx.push_back(b+2); idx.push_back(t+2);
    }
    return createMeshFromData(name, pos, norm, uv, idx);
}

MeshPtr MeshGenerator::generateCone(float radius, float height, int segments) {
    std::string name = "proc_cone_" + std::to_string(++sMeshGenCounter);
    std::vector<float> pos, norm, uv;
    std::vector<uint32_t> idx;

    // Apex
    pos.push_back(0); pos.push_back(height); pos.push_back(0);
    norm.push_back(0); norm.push_back(1); norm.push_back(0);
    uv.push_back(0.5f); uv.push_back(0);

    for (int i = 0; i <= segments; i++) {
        float u = (float)i / segments;
        float theta = u * 2.0f * Math::PI;
        float nx = std::cos(theta), nz = std::sin(theta);
        pos.push_back(radius*nx); pos.push_back(0); pos.push_back(radius*nz);
        norm.push_back(nx); norm.push_back(0.5f); norm.push_back(nz);
        uv.push_back(u); uv.push_back(1);
    }
    for (int i = 0; i < segments; i++) {
        idx.push_back(0); idx.push_back(i+1); idx.push_back(i+2);
    }
    return createMeshFromData(name, pos, norm, uv, idx);
}

MeshPtr MeshGenerator::generatePlane(float width, float height, int segX, int segY) {
    std::string name = "proc_plane_" + std::to_string(++sMeshGenCounter);
    std::vector<float> pos, norm, uv;
    std::vector<uint32_t> idx;

    for (int y = 0; y <= segY; y++) {
        for (int x = 0; x <= segX; x++) {
            float u = (float)x / segX;
            float v = (float)y / segY;
            pos.push_back((u - 0.5f) * width); pos.push_back(0); pos.push_back((v - 0.5f) * height);
            norm.push_back(0); norm.push_back(1); norm.push_back(0);
            uv.push_back(u); uv.push_back(v);
        }
    }
    for (int y = 0; y < segY; y++) {
        for (int x = 0; x < segX; x++) {
            uint32_t a = y * (segX + 1) + x;
            uint32_t b = a + segX + 1;
            idx.push_back(a); idx.push_back(b); idx.push_back(a + 1);
            idx.push_back(b); idx.push_back(b + 1); idx.push_back(a + 1);
        }
    }
    return createMeshFromData(name, pos, norm, uv, idx);
}

MeshPtr MeshGenerator::generateTorusKnot(int p, int q, float radius, float tubeRadius) {
    // Simplified torus knot: parametric curve
    std::string name = "proc_torusknot_" + std::to_string(++sMeshGenCounter);
    int tubeSegments = 64;
    int tubeSides = 16;
    std::vector<float> pos, norm, uv;
    std::vector<uint32_t> idx;

    auto knotPos = [&](float t) -> Vector3 {
        float r = 2.0f + std::cos(q * t);
        return Vector3(r * std::cos(p * t), r * std::sin(p * t), -std::sin(q * t)) * radius;
    };

    for (int i = 0; i <= tubeSegments; i++) {
        float t = (float)i / tubeSegments * 2.0f * Math::PI;
        Vector3 center = knotPos(t);
        Vector3 next = knotPos(t + 0.01f);
        Vector3 tangent = (next - center).normalisedCopy();
        Vector3 up = Vector3::UNIT_Y;
        Vector3 binormal = tangent.crossProduct(up).normalisedCopy();
        Vector3 normal = binormal.crossProduct(tangent).normalisedCopy();

        for (int j = 0; j <= tubeSides; j++) {
            float phi = (float)j / tubeSides * 2.0f * Math::PI;
            Vector3 pt = center + tubeRadius * (std::cos(phi) * normal + std::sin(phi) * binormal);
            Vector3 n = (pt - center).normalisedCopy();
            pos.push_back(pt.x); pos.push_back(pt.y); pos.push_back(pt.z);
            norm.push_back(n.x); norm.push_back(n.y); norm.push_back(n.z);
            uv.push_back((float)i/tubeSegments); uv.push_back((float)j/tubeSides);
        }
    }
    for (int i = 0; i < tubeSegments; i++) {
        for (int j = 0; j < tubeSides; j++) {
            uint32_t a = i * (tubeSides + 1) + j;
            uint32_t b = a + tubeSides + 1;
            idx.push_back(a); idx.push_back(b); idx.push_back(a + 1);
            idx.push_back(b); idx.push_back(b + 1); idx.push_back(a + 1);
        }
    }
    return createMeshFromData(name, pos, norm, uv, idx);
}

MeshPtr MeshGenerator::generateGeoEllipse(int frequency, float eccentricity) {
    // Geosphere with eccentricity applied to Y axis
    auto mesh = generateGeosphere(frequency);
    // Note: actual eccentricity deformation would need vertex modification
    // For now, returns a standard geosphere (user can scale Y via SceneNode)
    return mesh;
}

MeshPtr MeshGenerator::generateCube(float size, int segments) {
    std::string name = "proc_cube_" + std::to_string(++sMeshGenCounter);
    std::vector<float> pos, norm, uv;
    std::vector<uint32_t> idx;
    float h = size * 0.5f;

    // 6 faces: +Y, -Y, +X, -X, +Z, -Z
    struct Face { Vector3 normal; Vector3 right; Vector3 up; };
    Face faces[] = {
        {{ 0, 1, 0}, { 1, 0, 0}, { 0, 0,-1}},  // +Y (top)
        {{ 0,-1, 0}, { 1, 0, 0}, { 0, 0, 1}},  // -Y (bottom)
        {{ 1, 0, 0}, { 0, 0,-1}, { 0, 1, 0}},  // +X (right)
        {{-1, 0, 0}, { 0, 0, 1}, { 0, 1, 0}},  // -X (left)
        {{ 0, 0, 1}, { 1, 0, 0}, { 0, 1, 0}},  // +Z (front)
        {{ 0, 0,-1}, {-1, 0, 0}, { 0, 1, 0}},  // -Z (back)
    };

    for (auto& f : faces) {
        uint32_t base = static_cast<uint32_t>(pos.size() / 3);
        for (int sy = 0; sy <= segments; sy++) {
            for (int sx = 0; sx <= segments; sx++) {
                float u = (float)sx / segments;
                float v = (float)sy / segments;
                Vector3 p = f.normal * h
                    + f.right * ((u - 0.5f) * size)
                    + f.up * ((v - 0.5f) * size);
                pos.push_back(p.x); pos.push_back(p.y); pos.push_back(p.z);
                norm.push_back(f.normal.x); norm.push_back(f.normal.y); norm.push_back(f.normal.z);
                uv.push_back(u); uv.push_back(v);
            }
        }
        for (int sy = 0; sy < segments; sy++) {
            for (int sx = 0; sx < segments; sx++) {
                uint32_t a = base + sy * (segments + 1) + sx;
                uint32_t b = a + segments + 1;
                idx.push_back(a); idx.push_back(b); idx.push_back(a + 1);
                idx.push_back(b); idx.push_back(b + 1); idx.push_back(a + 1);
            }
        }
    }
    return createMeshFromData(name, pos, norm, uv, idx);
}

// Helper to create a named mesh, skipping if already exists in MeshManager.
static MeshPtr createNamedMesh(const std::string& name,
    const std::vector<float>& positions,
    const std::vector<float>& normals,
    const std::vector<float>& uvs,
    const std::vector<uint32_t>& indices)
{
    if (MeshManager::getSingleton().resourceExists(name,
            ResourceGroupManager::DEFAULT_RESOURCE_GROUP_NAME))
        return MeshManager::getSingleton().getByName(name,
            ResourceGroupManager::DEFAULT_RESOURCE_GROUP_NAME);
    return createMeshFromData(name, positions, normals, uvs, indices);
}

void MeshGenerator::registerDefaults() {
    // Generate each canonical mesh with its expected name.
    // Uses the same geometry algorithms as the generate* methods.

    auto& mgr = MeshManager::getSingleton();
    auto grp = ResourceGroupManager::DEFAULT_RESOURCE_GROUP_NAME;

    // ── torus.mesh ──────────────────────────────────────────────────
    if (!mgr.resourceExists("torus.mesh", grp)) {
        float majorR = 1.0f, minorR = 0.3f;
        int rings = 32, segs = 24;
        std::vector<float> pos, norm, uv;
        std::vector<uint32_t> idx;
        for (int i = 0; i <= rings; i++) {
            float u = (float)i / rings;
            float theta = u * 2.0f * Math::PI;
            for (int j = 0; j <= segs; j++) {
                float v = (float)j / segs;
                float phi = v * 2.0f * Math::PI;
                float px = (majorR + minorR * std::cos(phi)) * std::cos(theta);
                float py = minorR * std::sin(phi);
                float pz = (majorR + minorR * std::cos(phi)) * std::sin(theta);
                float nx = std::cos(phi) * std::cos(theta);
                float ny = std::sin(phi);
                float nz = std::cos(phi) * std::sin(theta);
                pos.push_back(px); pos.push_back(py); pos.push_back(pz);
                norm.push_back(nx); norm.push_back(ny); norm.push_back(nz);
                uv.push_back(u); uv.push_back(v);
            }
        }
        for (int i = 0; i < rings; i++) {
            for (int j = 0; j < segs; j++) {
                uint32_t a = i * (segs + 1) + j;
                uint32_t b = a + segs + 1;
                idx.push_back(a); idx.push_back(b); idx.push_back(a + 1);
                idx.push_back(b); idx.push_back(b + 1); idx.push_back(a + 1);
            }
        }
        createMeshFromData("torus.mesh", pos, norm, uv, idx);
    }

    // ── cylinder.mesh ───────────────────────────────────────────────
    if (!mgr.resourceExists("cylinder.mesh", grp)) {
        float radius = 0.5f, height = 2.0f;
        int segs = 24;
        float halfH = height * 0.5f;
        std::vector<float> pos, norm, uv;
        std::vector<uint32_t> idx;
        for (int i = 0; i <= segs; i++) {
            float u = (float)i / segs;
            float theta = u * 2.0f * Math::PI;
            float nx = std::cos(theta), nz = std::sin(theta);
            pos.push_back(radius*nx); pos.push_back(-halfH); pos.push_back(radius*nz);
            norm.push_back(nx); norm.push_back(0); norm.push_back(nz);
            uv.push_back(u); uv.push_back(1);
            pos.push_back(radius*nx); pos.push_back(halfH); pos.push_back(radius*nz);
            norm.push_back(nx); norm.push_back(0); norm.push_back(nz);
            uv.push_back(u); uv.push_back(0);
        }
        for (int i = 0; i < segs; i++) {
            uint32_t b = i * 2, t = b + 1;
            idx.push_back(b); idx.push_back(b+2); idx.push_back(t);
            idx.push_back(t); idx.push_back(b+2); idx.push_back(t+2);
        }
        createMeshFromData("cylinder.mesh", pos, norm, uv, idx);
    }

    // ── cone.mesh ───────────────────────────────────────────────────
    if (!mgr.resourceExists("cone.mesh", grp)) {
        float radius = 0.5f, height = 1.5f;
        int segs = 24;
        std::vector<float> pos, norm, uv;
        std::vector<uint32_t> idx;
        pos.push_back(0); pos.push_back(height); pos.push_back(0);
        norm.push_back(0); norm.push_back(1); norm.push_back(0);
        uv.push_back(0.5f); uv.push_back(0);
        for (int i = 0; i <= segs; i++) {
            float u = (float)i / segs;
            float theta = u * 2.0f * Math::PI;
            float nx = std::cos(theta), nz = std::sin(theta);
            pos.push_back(radius*nx); pos.push_back(0); pos.push_back(radius*nz);
            norm.push_back(nx); norm.push_back(0.5f); norm.push_back(nz);
            uv.push_back(u); uv.push_back(1);
        }
        for (int i = 0; i < segs; i++) {
            idx.push_back(0); idx.push_back(i+1); idx.push_back(i+2);
        }
        createMeshFromData("cone.mesh", pos, norm, uv, idx);
    }

    // ── plane_1m.mesh ───────────────────────────────────────────────
    if (!mgr.resourceExists("plane_1m.mesh", grp)) {
        float w = 1.0f, h = 1.0f;
        int segX = 1, segY = 1;
        std::vector<float> pos, norm, uv;
        std::vector<uint32_t> idx;
        for (int y = 0; y <= segY; y++) {
            for (int x = 0; x <= segX; x++) {
                float u = (float)x / segX;
                float v = (float)y / segY;
                pos.push_back((u - 0.5f) * w); pos.push_back(0); pos.push_back((v - 0.5f) * h);
                norm.push_back(0); norm.push_back(1); norm.push_back(0);
                uv.push_back(u); uv.push_back(v);
            }
        }
        for (int y = 0; y < segY; y++) {
            for (int x = 0; x < segX; x++) {
                uint32_t a = y * (segX + 1) + x;
                uint32_t b = a + segX + 1;
                idx.push_back(a); idx.push_back(b); idx.push_back(a + 1);
                idx.push_back(b); idx.push_back(b + 1); idx.push_back(a + 1);
            }
        }
        createMeshFromData("plane_1m.mesh", pos, norm, uv, idx);
    }

    // ── torusknot.mesh ──────────────────────────────────────────────
    if (!mgr.resourceExists("torusknot.mesh", grp)) {
        int p = 2, q = 3;
        float radius = 0.4f, tubeR = 0.1f;
        int tubeSegs = 64, tubeSides = 16;
        std::vector<float> pos, norm, uv;
        std::vector<uint32_t> idx;
        auto knotPos = [&](float t) -> Vector3 {
            float r = 2.0f + std::cos(q * t);
            return Vector3(r * std::cos(p * t), r * std::sin(p * t), -std::sin(q * t)) * radius;
        };
        for (int i = 0; i <= tubeSegs; i++) {
            float t = (float)i / tubeSegs * 2.0f * Math::PI;
            Vector3 center = knotPos(t);
            Vector3 next = knotPos(t + 0.01f);
            Vector3 tangent = (next - center).normalisedCopy();
            Vector3 up = Vector3::UNIT_Y;
            Vector3 binormal = tangent.crossProduct(up).normalisedCopy();
            Vector3 normal = binormal.crossProduct(tangent).normalisedCopy();
            for (int j = 0; j <= tubeSides; j++) {
                float phi = (float)j / tubeSides * 2.0f * Math::PI;
                Vector3 pt = center + tubeR * (std::cos(phi) * normal + std::sin(phi) * binormal);
                Vector3 n = (pt - center).normalisedCopy();
                pos.push_back(pt.x); pos.push_back(pt.y); pos.push_back(pt.z);
                norm.push_back(n.x); norm.push_back(n.y); norm.push_back(n.z);
                uv.push_back((float)i/tubeSegs); uv.push_back((float)j/tubeSides);
            }
        }
        for (int i = 0; i < tubeSegs; i++) {
            for (int j = 0; j < tubeSides; j++) {
                uint32_t a = i * (tubeSides + 1) + j;
                uint32_t b = a + tubeSides + 1;
                idx.push_back(a); idx.push_back(b); idx.push_back(a + 1);
                idx.push_back(b); idx.push_back(b + 1); idx.push_back(a + 1);
            }
        }
        createMeshFromData("torusknot.mesh", pos, norm, uv, idx);
    }

    // ── cube_1m.mesh ────────────────────────────────────────────────
    if (!mgr.resourceExists("cube_1m.mesh", grp)) {
        float size = 1.0f, h = 0.5f;
        std::vector<float> pos, norm, uv;
        std::vector<uint32_t> idx;
        struct Face { Vector3 n; Vector3 r; Vector3 u; };
        Face faces[] = {
            {{ 0, 1, 0}, { 1, 0, 0}, { 0, 0,-1}},
            {{ 0,-1, 0}, { 1, 0, 0}, { 0, 0, 1}},
            {{ 1, 0, 0}, { 0, 0,-1}, { 0, 1, 0}},
            {{-1, 0, 0}, { 0, 0, 1}, { 0, 1, 0}},
            {{ 0, 0, 1}, { 1, 0, 0}, { 0, 1, 0}},
            {{ 0, 0,-1}, {-1, 0, 0}, { 0, 1, 0}},
        };
        for (auto& f : faces) {
            uint32_t base = static_cast<uint32_t>(pos.size() / 3);
            for (int v = 0; v <= 1; v++) {
                for (int u = 0; u <= 1; u++) {
                    Vector3 p = f.n * h + f.r * ((u - 0.5f) * size) + f.u * ((v - 0.5f) * size);
                    pos.push_back(p.x); pos.push_back(p.y); pos.push_back(p.z);
                    norm.push_back(f.n.x); norm.push_back(f.n.y); norm.push_back(f.n.z);
                    uv.push_back((float)u); uv.push_back((float)v);
                }
            }
            idx.push_back(base); idx.push_back(base+2); idx.push_back(base+1);
            idx.push_back(base+2); idx.push_back(base+3); idx.push_back(base+1);
        }
        createMeshFromData("cube_1m.mesh", pos, norm, uv, idx);
    }

    // ── bbfx_plane.mesh ─────────────────────────────────────────────
    if (!mgr.resourceExists("bbfx_plane.mesh", grp)) {
        // 1x1 plane with good UV coverage, used by ShaderPreviewRenderer
        int segX = 4, segY = 4;
        float w = 1.0f, h = 1.0f;
        std::vector<float> pos, norm, uv;
        std::vector<uint32_t> idx;
        for (int y = 0; y <= segY; y++) {
            for (int x = 0; x <= segX; x++) {
                float u = (float)x / segX;
                float v = (float)y / segY;
                pos.push_back((u - 0.5f) * w); pos.push_back(0); pos.push_back((v - 0.5f) * h);
                norm.push_back(0); norm.push_back(1); norm.push_back(0);
                uv.push_back(u); uv.push_back(v);
            }
        }
        for (int y = 0; y < segY; y++) {
            for (int x = 0; x < segX; x++) {
                uint32_t a = y * (segX + 1) + x;
                uint32_t b = a + segX + 1;
                idx.push_back(a); idx.push_back(b); idx.push_back(a + 1);
                idx.push_back(b); idx.push_back(b + 1); idx.push_back(a + 1);
            }
        }
        createMeshFromData("bbfx_plane.mesh", pos, norm, uv, idx);
    }

    // ── mobius.mesh ─────────────────────────────────────────────────
    if (!mgr.resourceExists("mobius.mesh", grp)) {
        generateMobius(1.0f, 0.4f, 64, 16);
    }

    // ── lissajous.mesh ──────────────────────────────────────────────
    if (!mgr.resourceExists("lissajous.mesh", grp)) {
        generateLissajous(3, 2, 5, 0.8f, 256, 8);
    }

    // ── helix.mesh ──────────────────────────────────────────────────
    if (!mgr.resourceExists("helix.mesh", grp)) {
        generateHelix(0.5f, 0.3f, 3, 128, 8);
    }

    // ── diamond.mesh ────────────────────────────────────────────────
    if (!mgr.resourceExists("diamond.mesh", grp)) {
        generateDiamond(0.5f, 1.5f, 8);
    }

    // ── star3d.mesh ─────────────────────────────────────────────────
    if (!mgr.resourceExists("star3d.mesh", grp)) {
        generateStar3D(1.0f, 0.4f, 5, 0.3f);
    }
}

// ── Mobius strip ─────────────────────────────────────────────────────
MeshPtr MeshGenerator::generateMobius(float radius, float width, int segments, int sides) {
    std::string name = "mobius.mesh";
    auto& mgr = MeshManager::getSingleton();
    auto grp = ResourceGroupManager::DEFAULT_RESOURCE_GROUP_NAME;
    if (mgr.resourceExists(name, grp)) return mgr.getByName(name, grp);

    std::vector<float> pos, norm, uv;
    std::vector<uint32_t> idx;

    for (int i = 0; i <= segments; i++) {
        float u = (float)i / segments;
        float theta = u * 2.0f * Math::PI;
        for (int j = 0; j <= sides; j++) {
            float v = (float)j / sides;
            float s = (v - 0.5f) * width;
            // Mobius: half-twist = theta/2
            float halfT = theta * 0.5f;
            float px = (radius + s * std::cos(halfT)) * std::cos(theta);
            float py = (radius + s * std::cos(halfT)) * std::sin(theta);
            float pz = s * std::sin(halfT);
            pos.push_back(px); pos.push_back(py); pos.push_back(pz);
            // Approximate normal via cross product of tangents
            float nx = std::cos(halfT) * std::cos(theta);
            float ny = std::cos(halfT) * std::sin(theta);
            float nz = std::sin(halfT);
            float len = std::sqrt(nx*nx + ny*ny + nz*nz);
            if (len > 0.001f) { nx /= len; ny /= len; nz /= len; }
            norm.push_back(nx); norm.push_back(ny); norm.push_back(nz);
            uv.push_back(u); uv.push_back(v);
        }
    }
    for (int i = 0; i < segments; i++) {
        for (int j = 0; j < sides; j++) {
            uint32_t a = i * (sides + 1) + j;
            uint32_t b = a + sides + 1;
            idx.push_back(a); idx.push_back(b); idx.push_back(a + 1);
            idx.push_back(b); idx.push_back(b + 1); idx.push_back(a + 1);
        }
    }
    return createMeshFromData(name, pos, norm, uv, idx);
}

// ── Lissajous curve extruded as tube ────────────────────────────────
MeshPtr MeshGenerator::generateLissajous(int a, int b, int c, float scale, int segments, int sides) {
    std::string name = "lissajous.mesh";
    auto& mgr = MeshManager::getSingleton();
    auto grp = ResourceGroupManager::DEFAULT_RESOURCE_GROUP_NAME;
    if (mgr.resourceExists(name, grp)) return mgr.getByName(name, grp);

    float tubeR = 0.05f;
    std::vector<float> pos, norm, uv;
    std::vector<uint32_t> idx;

    // Compute center positions along the Lissajous curve
    auto curvePos = [&](float t) -> Vector3 {
        return Vector3(
            scale * std::sin((float)a * t),
            scale * std::sin((float)b * t + Math::PI * 0.5f),
            scale * std::sin((float)c * t)
        );
    };

    for (int i = 0; i <= segments; i++) {
        float t = (float)i / segments * 2.0f * Math::PI;
        float tNext = (float)(i + 1) / segments * 2.0f * Math::PI;
        Vector3 p = curvePos(t);
        Vector3 tangent = (curvePos(tNext) - curvePos(t - 0.01f)).normalisedCopy();
        // Build a frame (tangent, normal, binormal) via arbitrary up
        Vector3 up(0, 1, 0);
        if (std::abs(tangent.dotProduct(up)) > 0.99f) up = Vector3(1, 0, 0);
        Vector3 normal = tangent.crossProduct(up).normalisedCopy();
        Vector3 binormal = tangent.crossProduct(normal).normalisedCopy();

        float u = (float)i / segments;
        for (int j = 0; j <= sides; j++) {
            float v = (float)j / sides;
            float angle = v * 2.0f * Math::PI;
            Vector3 offset = normal * (tubeR * std::cos(angle)) + binormal * (tubeR * std::sin(angle));
            Vector3 pt = p + offset;
            Vector3 n = offset.normalisedCopy();
            pos.push_back(pt.x); pos.push_back(pt.y); pos.push_back(pt.z);
            norm.push_back(n.x); norm.push_back(n.y); norm.push_back(n.z);
            uv.push_back(u); uv.push_back(v);
        }
    }
    for (int i = 0; i < segments; i++) {
        for (int j = 0; j < sides; j++) {
            uint32_t aa = i * (sides + 1) + j;
            uint32_t bb = aa + sides + 1;
            idx.push_back(aa); idx.push_back(bb); idx.push_back(aa + 1);
            idx.push_back(bb); idx.push_back(bb + 1); idx.push_back(aa + 1);
        }
    }
    return createMeshFromData(name, pos, norm, uv, idx);
}

// ── Double helix (DNA-style) ────────────────────────────────────────
MeshPtr MeshGenerator::generateHelix(float radius, float pitch, int turns, int segments, int sides) {
    std::string name = "helix.mesh";
    auto& mgr = MeshManager::getSingleton();
    auto grp = ResourceGroupManager::DEFAULT_RESOURCE_GROUP_NAME;
    if (mgr.resourceExists(name, grp)) return mgr.getByName(name, grp);

    float tubeR = 0.04f;
    float totalHeight = pitch * turns;
    std::vector<float> pos, norm, uv;
    std::vector<uint32_t> idx;

    // Two strands
    for (int strand = 0; strand < 2; strand++) {
        float phaseOffset = strand * Math::PI;
        uint32_t baseVert = (uint32_t)(pos.size() / 3);
        for (int i = 0; i <= segments; i++) {
            float t = (float)i / segments;
            float theta = t * turns * 2.0f * Math::PI + phaseOffset;
            float y = t * totalHeight - totalHeight * 0.5f;
            Vector3 center(radius * std::cos(theta), y, radius * std::sin(theta));
            Vector3 tangent(-radius * std::sin(theta), totalHeight / (segments * 1.0f), radius * std::cos(theta));
            tangent.normalise();
            Vector3 up(0, 1, 0);
            Vector3 normal = tangent.crossProduct(up).normalisedCopy();
            Vector3 binormal = tangent.crossProduct(normal).normalisedCopy();

            for (int j = 0; j <= sides; j++) {
                float v = (float)j / sides;
                float angle = v * 2.0f * Math::PI;
                Vector3 offset = normal * (tubeR * std::cos(angle)) + binormal * (tubeR * std::sin(angle));
                Vector3 pt = center + offset;
                Vector3 n = offset.normalisedCopy();
                pos.push_back(pt.x); pos.push_back(pt.y); pos.push_back(pt.z);
                norm.push_back(n.x); norm.push_back(n.y); norm.push_back(n.z);
                uv.push_back(t); uv.push_back(v);
            }
        }
        for (int i = 0; i < segments; i++) {
            for (int j = 0; j < sides; j++) {
                uint32_t a = baseVert + i * (sides + 1) + j;
                uint32_t b = a + sides + 1;
                idx.push_back(a); idx.push_back(b); idx.push_back(a + 1);
                idx.push_back(b); idx.push_back(b + 1); idx.push_back(a + 1);
            }
        }
    }
    return createMeshFromData(name, pos, norm, uv, idx);
}

// ── Diamond (faceted gem) ───────────────────────────────────────────
MeshPtr MeshGenerator::generateDiamond(float radius, float height, int segments) {
    std::string name = "diamond.mesh";
    auto& mgr = MeshManager::getSingleton();
    auto grp = ResourceGroupManager::DEFAULT_RESOURCE_GROUP_NAME;
    if (mgr.resourceExists(name, grp)) return mgr.getByName(name, grp);

    std::vector<float> pos, norm, uv;
    std::vector<uint32_t> idx;

    float topY = height * 0.4f;    // crown height
    float botY = -height * 0.6f;   // pavilion depth
    float girdle = 0.0f;           // girdle at y=0

    // Build as triangle fan from top, girdle ring, bottom
    // Top vertex
    uint32_t topIdx = 0;
    pos.push_back(0); pos.push_back(topY); pos.push_back(0);
    norm.push_back(0); norm.push_back(1); norm.push_back(0);
    uv.push_back(0.5f); uv.push_back(0);

    // Girdle vertices
    for (int i = 0; i <= segments; i++) {
        float angle = (float)i / segments * 2.0f * Math::PI;
        float px = radius * std::cos(angle);
        float pz = radius * std::sin(angle);
        pos.push_back(px); pos.push_back(girdle); pos.push_back(pz);
        float nx = std::cos(angle), nz = std::sin(angle);
        norm.push_back(nx); norm.push_back(0); norm.push_back(nz);
        uv.push_back((float)i / segments); uv.push_back(0.5f);
    }

    // Bottom vertex
    uint32_t botIdx = (uint32_t)(pos.size() / 3);
    pos.push_back(0); pos.push_back(botY); pos.push_back(0);
    norm.push_back(0); norm.push_back(-1); norm.push_back(0);
    uv.push_back(0.5f); uv.push_back(1);

    // Crown triangles (top → girdle)
    for (int i = 0; i < segments; i++) {
        uint32_t g1 = 1 + i;
        uint32_t g2 = 1 + i + 1;
        idx.push_back(topIdx); idx.push_back(g1); idx.push_back(g2);
    }
    // Pavilion triangles (girdle → bottom)
    for (int i = 0; i < segments; i++) {
        uint32_t g1 = 1 + i;
        uint32_t g2 = 1 + i + 1;
        idx.push_back(g1); idx.push_back(botIdx); idx.push_back(g2);
    }
    return createMeshFromData(name, pos, norm, uv, idx);
}

// ── 3D Star (extruded star polygon) ─────────────────────────────────
MeshPtr MeshGenerator::generateStar3D(float outerRadius, float innerRadius, int points, float depth) {
    std::string name = "star3d.mesh";
    auto& mgr = MeshManager::getSingleton();
    auto grp = ResourceGroupManager::DEFAULT_RESOURCE_GROUP_NAME;
    if (mgr.resourceExists(name, grp)) return mgr.getByName(name, grp);

    std::vector<float> pos, norm, uv;
    std::vector<uint32_t> idx;
    int totalPts = points * 2;
    float halfDepth = depth * 0.5f;

    // Front and back star faces
    for (int face = 0; face < 2; face++) {
        float z = (face == 0) ? halfDepth : -halfDepth;
        float nz = (face == 0) ? 1.0f : -1.0f;
        uint32_t centerIdx = (uint32_t)(pos.size() / 3);
        // Center vertex
        pos.push_back(0); pos.push_back(0); pos.push_back(z);
        norm.push_back(0); norm.push_back(0); norm.push_back(nz);
        uv.push_back(0.5f); uv.push_back(0.5f);
        // Star vertices
        for (int i = 0; i < totalPts; i++) {
            float angle = (float)i / totalPts * 2.0f * Math::PI - Math::HALF_PI;
            float r = (i % 2 == 0) ? outerRadius : innerRadius;
            pos.push_back(r * std::cos(angle)); pos.push_back(r * std::sin(angle)); pos.push_back(z);
            norm.push_back(0); norm.push_back(0); norm.push_back(nz);
            uv.push_back(0.5f + 0.5f * r / outerRadius * std::cos(angle));
            uv.push_back(0.5f + 0.5f * r / outerRadius * std::sin(angle));
        }
        // Triangles
        for (int i = 0; i < totalPts; i++) {
            uint32_t a = centerIdx + 1 + i;
            uint32_t b = centerIdx + 1 + (i + 1) % totalPts;
            if (face == 0) { idx.push_back(centerIdx); idx.push_back(a); idx.push_back(b); }
            else { idx.push_back(centerIdx); idx.push_back(b); idx.push_back(a); }
        }
    }

    // Side faces (connect front to back star edges)
    uint32_t frontStart = 1; // first star vertex on front face
    uint32_t backStart = 1 + totalPts + 1; // first star vertex on back face
    for (int i = 0; i < totalPts; i++) {
        uint32_t f1 = frontStart + i;
        uint32_t f2 = frontStart + (i + 1) % totalPts;
        uint32_t b1 = backStart + i;
        uint32_t b2 = backStart + (i + 1) % totalPts;
        // Two triangles per side quad
        idx.push_back(f1); idx.push_back(b1); idx.push_back(f2);
        idx.push_back(b1); idx.push_back(b2); idx.push_back(f2);
        // Side normals (approximate)
        // Already have position normals set, which will be face normals — acceptable for a faceted look
    }

    return createMeshFromData(name, pos, norm, uv, idx);
}

} // namespace bbfx
