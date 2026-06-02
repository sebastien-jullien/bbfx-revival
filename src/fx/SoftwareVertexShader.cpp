#include "SoftwareVertexShader.h"
#include <cstring>
#include <cmath>
#include <iostream>

#include <SDL3/SDL.h>

// GL types — avoid linking opengl32.lib in headless mode
typedef unsigned int GLenum;
typedef int GLint;
typedef unsigned int GLuint;
#ifndef APIENTRY
#ifdef _WIN32
#define APIENTRY __stdcall
#else
#define APIENTRY
#endif
#endif

// GL constants not in gl.h
#ifndef GL_COPY_READ_BUFFER
#define GL_COPY_READ_BUFFER 0x8F36
#endif
#ifndef GL_ARRAY_BUFFER_BINDING
#define GL_ARRAY_BUFFER_BINDING 0x8894
#endif
#ifndef GL_ELEMENT_ARRAY_BUFFER_BINDING
#define GL_ELEMENT_ARRAY_BUFFER_BINDING 0x8895
#endif
#ifndef GL_ARRAY_BUFFER
#define GL_ARRAY_BUFFER 0x8892
#endif

namespace bbfx {

using namespace Ogre;

// GL State Guard for readBufferRaw — saves/restores buffer bindings
// that OGRE's GL3Plus readData() clobbers without restoring.
static void* sGLBindBuffer = nullptr;
static void ensureGLGuard() {
    if (!sGLBindBuffer)
        sGLBindBuffer = SDL_GL_GetProcAddress("glBindBuffer");
}
// Load glGetIntegerv dynamically to avoid link error in headless mode
static void* sGLGetIntegerv = nullptr;
static void glGuardSave(GLint& copyRead, GLint& arrayBuf) {
    if (!sGLGetIntegerv) sGLGetIntegerv = SDL_GL_GetProcAddress("glGetIntegerv");
    if (!sGLGetIntegerv) return;
    using GetIntFn = void(APIENTRY*)(GLenum, GLint*);
    auto fn = reinterpret_cast<GetIntFn>(sGLGetIntegerv);
    fn(GL_COPY_READ_BUFFER, &copyRead);
    fn(GL_ARRAY_BUFFER_BINDING, &arrayBuf);
}
static void glGuardRestore(GLint copyRead, GLint arrayBuf) {
    ensureGLGuard();
    using BindBufFn = void(APIENTRY*)(unsigned int, unsigned int);
    auto bindBuf = reinterpret_cast<BindBufFn>(sGLBindBuffer);
    if (bindBuf) {
        bindBuf(GL_COPY_READ_BUFFER, static_cast<unsigned int>(copyRead));
        bindBuf(GL_ARRAY_BUFFER, static_cast<unsigned int>(arrayBuf));
    }
}

SoftwareVertexShader::SoftwareVertexShader(const String& meshName, const String& cloneName) {
    mMeshName = cloneName;
    _loadMesh(meshName);
}

SoftwareVertexShader::~SoftwareVertexShader() {
    disable();
    // Libère le mesh cloné (createManual) — les appelants détruisent l'entité avant
    // de détruire le shader, donc plus aucune entité ne référence ce mesh ici.
    if (clonedMesh) {
        std::string n = clonedMesh->getName();
        clonedMesh.reset();
        if (MeshManager::getSingletonPtr() && MeshManager::getSingleton().getByName(n))
            MeshManager::getSingleton().remove(n);
    }
}

bool SoftwareVertexShader::frameStarted(const FrameEvent& e) {
    // En mode DAG-driven, c'est le node propriétaire qui appelle renderOneFrame(dt)
    // depuis son port `dt` — on n'avance pas ici pour éviter le double-avancement.
    if (!mDagDrivenTime) renderOneFrame(e.timeSinceLastFrame);
    return true;
}

void SoftwareVertexShader::enable() {
    if (!mCloneReady) {
        _prepareClonedMesh();
        mCloneReady = true;
    }
    Root::getSingleton().addFrameListener(this);
}
void SoftwareVertexShader::disable() {
    if (Root::getSingletonPtr()) Root::getSingleton().removeFrameListener(this);
}

// Read ALL raw bytes from a vertex buffer with GL state protection
static std::vector<uint8_t> readBufferRaw(HardwareVertexBufferSharedPtr buf) {
    std::vector<uint8_t> data(buf->getSizeInBytes());
    GLint savedCopyRead = 0, savedArrayBuf = 0;
    glGuardSave(savedCopyRead, savedArrayBuf);
    if (buf->hasShadowBuffer()) {
        const void* p = buf->lock(0, buf->getSizeInBytes(), HardwareBuffer::HBL_READ_ONLY);
        std::memcpy(data.data(), p, data.size());
        buf->unlock();
    } else {
        buf->readData(0, data.size(), data.data());
    }
    glGuardRestore(savedCopyRead, savedArrayBuf);
    return data;
}

static std::vector<uint8_t> readIndexRaw(HardwareIndexBufferSharedPtr buf) {
    std::vector<uint8_t> data(buf->getSizeInBytes());
    GLint savedCopyRead = 0, savedArrayBuf = 0;
    glGuardSave(savedCopyRead, savedArrayBuf);
    if (buf->hasShadowBuffer()) {
        const void* p = buf->lock(0, buf->getSizeInBytes(), HardwareBuffer::HBL_READ_ONLY);
        std::memcpy(data.data(), p, data.size());
        buf->unlock();
    } else {
        buf->readData(0, data.size(), data.data());
    }
    glGuardRestore(savedCopyRead, savedArrayBuf);
    return data;
}

HardwareVertexBufferSharedPtr SoftwareVertexShader::_cloneBuffer(HardwareVertexBufferSharedPtr) {
    return {}; // Not used — cloning is done manually
}
bool SoftwareVertexShader::_isDynamic(VertexElementSemantic) { return true; }
void SoftwareVertexShader::_reorganizeVertexBuffers(VertexData*) {}
VertexData* SoftwareVertexShader::_prepareVertexData(VertexData*) { return nullptr; }
void SoftwareVertexShader::_extractPositions(VertexData*, CpuMeshData&) {}
void SoftwareVertexShader::_extractCpuData(VertexData*, IndexData*, CpuMeshData&) {}
void SoftwareVertexShader::_fillCloneFromCpu(VertexData*, VertexData*, const CpuMeshData&) {}

// Generate spherical UVs for clone meshes that lack VES_TEXTURE_COORDINATES.
// Without UVs, RTSS-generated shaders sample texture at (0,0) → black.
static void addTexCoordsIfMissing(VertexData* vd) {
    if (!vd) return;
    if (vd->vertexDeclaration->findElementBySemantic(VES_TEXTURE_COORDINATES)) return;

    auto* posElem = vd->vertexDeclaration->findElementBySemantic(VES_POSITION);
    if (!posElem) return;

    unsigned short posSrc = posElem->getSource();
    auto oldBuf = vd->vertexBufferBinding->getBuffer(posSrc);
    size_t oldVertSize = oldBuf->getVertexSize();
    size_t numVerts = oldBuf->getNumVertices();
    size_t newVertSize = oldVertSize + sizeof(float) * 2;

    auto newBuf = HardwareBufferManager::getSingleton().createVertexBuffer(
        newVertSize, numVerts, HBU_CPU_TO_GPU, true);

    const uint8_t* oldData = static_cast<const uint8_t*>(
        oldBuf->lock(HardwareBuffer::HBL_READ_ONLY));
    uint8_t* newData = static_cast<uint8_t*>(
        newBuf->lock(HardwareBuffer::HBL_DISCARD));

    size_t posOffset = posElem->getOffset();
    constexpr float PI = 3.14159265358979323846f;

    for (size_t v = 0; v < numVerts; v++) {
        // Copy original vertex data (POS + NORMAL + whatever else)
        std::memcpy(newData + v * newVertSize, oldData + v * oldVertSize, oldVertSize);

        // Read position, normalize, compute spherical UV
        const float* pos = reinterpret_cast<const float*>(
            oldData + v * oldVertSize + posOffset);
        float x = pos[0], y = pos[1], z = pos[2];
        float len = std::sqrt(x * x + y * y + z * z);
        if (len > 1e-6f) { x /= len; y /= len; z /= len; }

        float u = 0.5f + std::atan2(z, x) / (2.0f * PI);
        float vc = 0.5f - std::asin(std::max(-1.0f, std::min(1.0f, y))) / PI;

        float* uv = reinterpret_cast<float*>(newData + v * newVertSize + oldVertSize);
        uv[0] = u;
        uv[1] = vc;
    }

    oldBuf->unlock();
    newBuf->unlock();

    vd->vertexBufferBinding->setBinding(posSrc, newBuf);
    vd->vertexDeclaration->addElement(
        posSrc, oldVertSize, VET_FLOAT2, VES_TEXTURE_COORDINATES, 0);

    std::cout << "[SoftwareVS] Added spherical UVs: vertexSize " << oldVertSize
              << " -> " << newVertSize << " (" << numVerts << " verts)" << std::endl;
}

void SoftwareVertexShader::_prepareClonedMesh() {
    // Step 1: Read ALL vertex/index data from original mesh into CPU arrays
    // This happens once at load time. Shadow buffers ensure no GPU stall.

    struct BufferSnapshot {
        std::vector<uint8_t> data;
        size_t vertexSize;
        size_t numVertices;
    };

    // Snapshot all unique vertex buffers from original mesh
    auto snapshotVertexData = [](VertexData* vd) -> std::map<unsigned short, BufferSnapshot> {
        std::map<unsigned short, BufferSnapshot> snaps;
        if (!vd) return snaps;
        const auto& elems = vd->vertexDeclaration->getElements();
        for (const auto& e : elems) {
            unsigned short src = e.getSource();
            if (snaps.count(src)) continue;
            auto buf = vd->vertexBufferBinding->getBuffer(src);
            snaps[src] = { readBufferRaw(buf), buf->getVertexSize(), buf->getNumVertices() };
        }
        return snaps;
    };

    // Extract positions from a buffer snapshot
    auto extractPositions = [](VertexData* vd, const std::map<unsigned short, BufferSnapshot>& snaps, CpuMeshData& out) {
        if (!vd) return;
        const auto* posElem = vd->vertexDeclaration->findElementBySemantic(VES_POSITION);
        if (!posElem) return;
        auto it = snaps.find(posElem->getSource());
        if (it == snaps.end()) return;
        const auto& snap = it->second;
        out.vertexCount = snap.numVertices;
        out.positions.resize(out.vertexCount * 3);
        for (size_t v = 0; v < out.vertexCount; v++) {
            const float* src = reinterpret_cast<const float*>(
                snap.data.data() + v * snap.vertexSize + posElem->getOffset());
            out.positions[v*3]   = src[0];
            out.positions[v*3+1] = src[1];
            out.positions[v*3+2] = src[2];
        }
    };

    // Extract indices
    auto extractIndices = [](IndexData* id, CpuMeshData& out) {
        if (!id || !id->indexBuffer) return;
        auto ibuf = id->indexBuffer;
        out.indexCount = id->indexCount;
        out.indices.resize(out.indexCount);
        auto raw = readIndexRaw(ibuf);
        if (ibuf->getType() == HardwareIndexBuffer::IT_32BIT) {
            std::memcpy(out.indices.data(), raw.data(), out.indexCount * sizeof(uint32_t));
        } else {
            const uint16_t* idx16 = reinterpret_cast<const uint16_t*>(raw.data());
            for (size_t i = 0; i < out.indexCount; i++) out.indices[i] = idx16[i];
        }
    };

    // Snapshot shared vertex data
    auto sharedSnaps = snapshotVertexData(originalMesh->sharedVertexData);
    extractPositions(originalMesh->sharedVertexData, sharedSnaps, mSharedCpuData);

    // Snapshot per-submesh
    mSubMeshCpuData.resize(originalMesh->getNumSubMeshes());
    std::vector<std::map<unsigned short, BufferSnapshot>> subSnaps(originalMesh->getNumSubMeshes());

    for (unsigned sm = 0; sm < originalMesh->getNumSubMeshes(); sm++) {
        auto* sub0 = originalMesh->getSubMesh(sm);
        VertexData* srcVd = sub0->useSharedVertices ? originalMesh->sharedVertexData : sub0->vertexData;
        auto& snaps = sub0->useSharedVertices ? sharedSnaps : subSnaps[sm];
        if (!sub0->useSharedVertices) snaps = snapshotVertexData(sub0->vertexData);
        extractPositions(srcVd, snaps, mSubMeshCpuData[sm]);
        extractIndices(sub0->indexData, mSubMeshCpuData[sm]);
    }

    // Step 2: Create clone mesh with CPU_ONLY buffers, filled from snapshots
    clonedMesh = MeshManager::getSingleton().createManual(
        mMeshName, ResourceGroupManager::DEFAULT_RESOURCE_GROUP_NAME);
    clonedMesh->_setBounds(originalMesh->getBounds());
    clonedMesh->_setBoundingSphereRadius(originalMesh->getBoundingSphereRadius());

    auto createCloneVD = [](VertexData* orig, const std::map<unsigned short, BufferSnapshot>& snaps) -> VertexData* {
        if (!orig) return nullptr;
        auto* vd = new VertexData();
        vd->vertexCount = orig->vertexCount;
        vd->vertexStart = orig->vertexStart;
        std::set<unsigned short> done;
        for (const auto& elem : orig->vertexDeclaration->getElements()) {
            unsigned short src = elem.getSource();
            if (!done.count(src)) {
                auto it = snaps.find(src);
                if (it != snaps.end()) {
                    auto buf = HardwareBufferManager::getSingleton().createVertexBuffer(
                        it->second.vertexSize, it->second.numVertices, HBU_CPU_TO_GPU, true);
                    // Write snapshot data into the new CPU buffer
                    void* dst = buf->lock(HardwareBuffer::HBL_DISCARD);
                    std::memcpy(dst, it->second.data.data(), it->second.data.size());
                    buf->unlock();
                    vd->vertexBufferBinding->setBinding(src, buf);
                }
                done.insert(src);
            }
            vd->vertexDeclaration->addElement(src, elem.getOffset(),
                elem.getType(), elem.getSemantic(), elem.getIndex());
        }
        return vd;
    };

    clonedMesh->sharedVertexData = createCloneVD(originalMesh->sharedVertexData, sharedSnaps);
    addTexCoordsIfMissing(clonedMesh->sharedVertexData);

    std::cout << "[SoftwareVS] _prepareClonedMesh: orig='" << originalMesh->getName()
              << "' clone='" << mMeshName << "'"
              << " numSubs=" << originalMesh->getNumSubMeshes()
              << " hasSharedVD=" << (originalMesh->sharedVertexData != nullptr) << std::endl;

    for (unsigned sm = 0; sm < originalMesh->getNumSubMeshes(); sm++) {
        auto* sub0 = originalMesh->getSubMesh(sm);
        auto* sub = clonedMesh->createSubMesh();
        if (!sub0->getMaterialName().empty()) sub->setMaterialName(sub0->getMaterialName());
        sub->useSharedVertices = sub0->useSharedVertices;
        if (!sub0->useSharedVertices) {
            sub->vertexData = createCloneVD(sub0->vertexData, subSnaps[sm]);
            addTexCoordsIfMissing(sub->vertexData);
        }
        sub->indexData->indexBuffer = sub0->indexData->indexBuffer;
        sub->indexData->indexStart = sub0->indexData->indexStart;
        sub->indexData->indexCount = sub0->indexData->indexCount;

        std::cout << "[SoftwareVS]   sub[" << sm << "] useShared=" << sub0->useSharedVertices
                  << " mat='" << sub0->getMaterialName() << "'"
                  << " idxCount=" << sub0->indexData->indexCount;
        // Log vertex data details
        VertexData* srcVd = sub0->useSharedVertices ? originalMesh->sharedVertexData : sub0->vertexData;
        if (srcVd) {
            auto posElem = srcVd->vertexDeclaration->findElementBySemantic(VES_POSITION);
            if (posElem) {
                auto buf = srcVd->vertexBufferBinding->getBuffer(posElem->getSource());
                std::cout << " bufVertSize=" << buf->getVertexSize()
                          << " bufNumVerts=" << buf->getNumVertices()
                          << " shadow=" << buf->hasShadowBuffer();
            }
        }
        std::cout << std::endl;
        // Log CPU data
        std::cout << "[SoftwareVS]   cpuData[" << sm << "] verts=" << mSubMeshCpuData[sm].vertexCount
                  << " posSize=" << mSubMeshCpuData[sm].positions.size()
                  << " idxSize=" << mSubMeshCpuData[sm].indices.size() << std::endl;
    }

    // Finalize the manual mesh — required for GL3Plus to properly set up
    // internal buffer bindings, vertex declaration compilation, and resource state.
    clonedMesh->load();
}

void SoftwareVertexShader::_loadMesh(const String& meshName) {
    // Simply get or load the mesh — no unload, no remove, no special params.
    // readBufferRaw() has GL state guards to handle readData() safely.
    originalMesh = MeshManager::getSingleton().getByName(meshName);
    if (!originalMesh) {
        originalMesh = MeshManager::getSingleton().load(
            meshName, ResourceGroupManager::DEFAULT_RESOURCE_GROUP_NAME);
    }
    assert(originalMesh);
    mCloneReady = false;
}

} // namespace bbfx
