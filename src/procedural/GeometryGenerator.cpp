#include "GeometryGenerator.h"

#include <iostream>

#include <OgreHardwareVertexBuffer.h>
#include <OgreManualObject.h>
#include <OgreMesh.h>
#include <OgreMeshManager.h>
#include <OgreResourceGroupManager.h>
#include <OgreSubMesh.h>
#include <OgreVertexIndexData.h>

#include "../studio/generators/MeshGenerator.h"

namespace bbfx {

namespace {

constexpr size_t kFloatsPerVertex = 8;  // pos3 + normal3 + uv2

void setMeshName(Ogre::MeshPtr mesh, const std::string& name) {
    if (!mesh) return;
    // OGRE does not expose a "rename" API, but we can re-register a
    // shallow clone under the requested name if it differs.
    if (mesh->getName() == name) return;
    Ogre::MeshPtr aliased = mesh->clone(name);
    (void)aliased;
}

} // anonymous

std::string GeometryGenerator::createMesh(const std::string& name,
                                                const std::vector<float>& vertices,
                                                const std::vector<uint32_t>& indices,
                                                const std::string& material) {
    if (name.empty() || vertices.empty() || indices.empty()) return {};
    if (vertices.size() % kFloatsPerVertex != 0) {
        std::cerr << "[GeometryGenerator] createMesh(" << name
                   << "): vertex buffer size "
                   << vertices.size() << " is not a multiple of 8" << std::endl;
        return {};
    }
    try {
        Ogre::ManualObject mo(name);
        std::string mat = material.empty() ? "BaseWhiteNoLighting" : material;
        mo.begin(mat, Ogre::RenderOperation::OT_TRIANGLE_LIST);
        size_t vcount = vertices.size() / kFloatsPerVertex;
        for (size_t i = 0; i < vcount; ++i) {
            const float* v = &vertices[i * kFloatsPerVertex];
            mo.position(v[0], v[1], v[2]);
            mo.normal  (v[3], v[4], v[5]);
            mo.textureCoord(v[6], v[7]);
        }
        for (size_t i = 0; i + 2 < indices.size(); i += 3) {
            mo.triangle(indices[i], indices[i + 1], indices[i + 2]);
        }
        mo.end();
        auto mesh = mo.convertToMesh(name,
                     Ogre::ResourceGroupManager::DEFAULT_RESOURCE_GROUP_NAME);
        if (mesh) return name;
    } catch (const std::exception& e) {
        std::cerr << "[GeometryGenerator] createMesh(" << name << ") failed: "
                   << e.what() << std::endl;
    }
    return {};
}

bool GeometryGenerator::updateVertices(const std::string& meshName,
                                             const std::vector<float>& vertices) {
    if (meshName.empty() || vertices.empty()) return false;
    auto& mm = Ogre::MeshManager::getSingleton();
    auto mesh = mm.getByName(meshName);
    if (!mesh || mesh->getNumSubMeshes() == 0) return false;

    auto* sub = mesh->getSubMesh(0);
    Ogre::VertexData* vd = sub->useSharedVertices ? mesh->sharedVertexData : sub->vertexData;
    if (!vd) return false;
    size_t vcount = vd->vertexCount;
    if (vertices.size() != vcount * kFloatsPerVertex) {
        std::cerr << "[GeometryGenerator] updateVertices: size mismatch for "
                   << meshName << " — expected " << (vcount * kFloatsPerVertex)
                   << ", got " << vertices.size() << std::endl;
        return false;
    }
    // We only update the position buffer here (binding 0). For a full
    // pipeline that also refreshes normals/uvs, the caller can ship a
    // completely new mesh via createMesh instead.
    auto binding = vd->vertexBufferBinding;
    if (!binding || binding->getBufferCount() == 0) return false;
    auto buf = binding->getBuffer(0);
    if (!buf) return false;
    size_t stride = buf->getVertexSize();
    if (stride != sizeof(float) * 3 && stride != sizeof(float) * 8) {
        std::cerr << "[GeometryGenerator] updateVertices: unsupported stride "
                   << stride << " on mesh " << meshName << std::endl;
        return false;
    }
    try {
        float* dst = static_cast<float*>(buf->lock(Ogre::HardwareBuffer::HBL_DISCARD));
        size_t perVert = stride / sizeof(float);
        for (size_t i = 0; i < vcount; ++i) {
            const float* src = &vertices[i * kFloatsPerVertex];
            // position always first three floats
            dst[i * perVert + 0] = src[0];
            dst[i * perVert + 1] = src[1];
            dst[i * perVert + 2] = src[2];
            if (perVert >= 8) {
                dst[i * perVert + 3] = src[3];
                dst[i * perVert + 4] = src[4];
                dst[i * perVert + 5] = src[5];
                dst[i * perVert + 6] = src[6];
                dst[i * perVert + 7] = src[7];
            }
        }
        buf->unlock();
        return true;
    } catch (const std::exception& e) {
        std::cerr << "[GeometryGenerator] updateVertices failed: " << e.what() << std::endl;
        return false;
    }
}

std::string GeometryGenerator::createSphere(const std::string& name, float /*radius*/,
                                                   int rings, int segments) {
    auto mesh = MeshGenerator::generateSphere(rings, segments);
    setMeshName(mesh, name);
    return mesh ? name : std::string();
}
std::string GeometryGenerator::createPlane(const std::string& name, float w, float h,
                                                 int segX, int segY) {
    auto mesh = MeshGenerator::generatePlane(w, h, segX, segY);
    setMeshName(mesh, name);
    return mesh ? name : std::string();
}
std::string GeometryGenerator::createCylinder(const std::string& name, float radius,
                                                    float height, int segments) {
    auto mesh = MeshGenerator::generateCylinder(radius, height, segments);
    setMeshName(mesh, name);
    return mesh ? name : std::string();
}
std::string GeometryGenerator::createTorus(const std::string& name,
                                                 float majorRadius, float minorRadius,
                                                 int rings, int segments) {
    auto mesh = MeshGenerator::generateTorus(majorRadius, minorRadius, rings, segments);
    setMeshName(mesh, name);
    return mesh ? name : std::string();
}

} // namespace bbfx
