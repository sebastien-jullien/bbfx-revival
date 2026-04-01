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

} // namespace bbfx
