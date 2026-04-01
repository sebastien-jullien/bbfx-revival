#include "SoftwareVertexShader.h"
#include <cstring>
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

SoftwareVertexShader::~SoftwareVertexShader() { disable(); }

bool SoftwareVertexShader::frameStarted(const FrameEvent& e) {
    if (!mCloneReady) {
        // Do NOT unload/remove/reload — it destroys buffers used by other Entities.
        // readBufferRaw() has GL state guards to handle readData() safely.
        _prepareClonedMesh();
        mCloneReady = true;
    }
    renderOneFrame(e.timeSinceLastFrame);
    return true;
}

void SoftwareVertexShader::enable() { Root::getSingleton().addFrameListener(this); }
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
                        it->second.vertexSize, it->second.numVertices, HBU_CPU_TO_GPU, false);
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

    for (unsigned sm = 0; sm < originalMesh->getNumSubMeshes(); sm++) {
        auto* sub0 = originalMesh->getSubMesh(sm);
        auto* sub = clonedMesh->createSubMesh();
        if (!sub0->getMaterialName().empty()) sub->setMaterialName(sub0->getMaterialName());
        sub->useSharedVertices = sub0->useSharedVertices;
        if (!sub0->useSharedVertices) {
            sub->vertexData = createCloneVD(sub0->vertexData, subSnaps[sm]);
        }
        sub->indexData->indexBuffer = sub0->indexData->indexBuffer;
        sub->indexData->indexStart = sub0->indexData->indexStart;
        sub->indexData->indexCount = sub0->indexData->indexCount;
    }
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
