#include "NdiOutputNode.h"
#include "../../core/AnimationPort.h"
#include <OgreRenderTexture.h>
#include <iostream>
#include <vector>
#include <algorithm>

// GL headers for PBO (fallback: no PBO if GL not available)
#ifdef _WIN32
#  define NOMINMAX
#  include <windows.h>
#  include <GL/gl.h>
   // GL_PIXEL_PACK_BUFFER extension
#  ifndef GL_PIXEL_PACK_BUFFER
#    define GL_PIXEL_PACK_BUFFER 0x88EB
#  endif
#  ifndef GL_STREAM_READ
#    define GL_STREAM_READ 0x88E1
#  endif
#  ifndef GL_READ_ONLY
#    define GL_READ_ONLY 0x88B8
#  endif
#  ifndef GL_BGRA
#    define GL_BGRA 0x80E1
#  endif
#else
#  include <GL/gl.h>
#  include <GL/glext.h>
#endif

#ifdef BBFX_HAS_NDI
#  include <Processing.NDI.Lib.h>
#endif

namespace bbfx {

// ── GL extension function pointers (PBO) ──────────────────────────────────────

#ifdef _WIN32
typedef void  (APIENTRY* PFNGLGENBUFFERSPROC)   (GLsizei n, GLuint* bufs);
typedef void  (APIENTRY* PFNGLDELETEBUFFERSPROC) (GLsizei n, const GLuint* bufs);
typedef void  (APIENTRY* PFNGLBINDBUFFERPROC)    (GLenum target, GLuint buf);
typedef void  (APIENTRY* PFNGLBUFFERDATAPROC)    (GLenum target, ptrdiff_t size, const void* data, GLenum usage);
typedef void* (APIENTRY* PFNGLMAPBUFFERPROC)     (GLenum target, GLenum access);
typedef GLboolean (APIENTRY* PFNGLUNMAPBUFFERPROC) (GLenum target);

static PFNGLGENBUFFERSPROC   sNdiGenBuffers   = nullptr;
static PFNGLDELETEBUFFERSPROC sNdiDelBuffers  = nullptr;
static PFNGLBINDBUFFERPROC   sNdiBindBuffer   = nullptr;
static PFNGLBUFFERDATAPROC   sNdiBufferData   = nullptr;
static PFNGLMAPBUFFERPROC    sNdiMapBuffer    = nullptr;
static PFNGLUNMAPBUFFERPROC  sNdiUnmapBuffer  = nullptr;

static bool sNdiGLLoaded = false;

static void loadNdiGL() {
    if (sNdiGLLoaded) return;
#define LOAD(T, name) s##name = reinterpret_cast<T>(wglGetProcAddress(#name));
    LOAD(PFNGLGENBUFFERSPROC,    NdiGenBuffers)
    LOAD(PFNGLDELETEBUFFERSPROC, NdiDelBuffers)
    LOAD(PFNGLBINDBUFFERPROC,    NdiBindBuffer)
    LOAD(PFNGLBUFFERDATAPROC,    NdiBufferData)
    LOAD(PFNGLMAPBUFFERPROC,     NdiMapBuffer)
    LOAD(PFNGLUNMAPBUFFERPROC,   NdiUnmapBuffer)
#undef LOAD
    sNdiGLLoaded = (sNdiGenBuffers && sNdiDelBuffers && sNdiBindBuffer &&
                    sNdiBufferData && sNdiMapBuffer && sNdiUnmapBuffer);
    if (!sNdiGLLoaded)
        std::cerr << "[NDI] PBO GL extension functions not available" << std::endl;
}
#endif // _WIN32

// ── NdiOutputNode ─────────────────────────────────────────────────────────────

NdiOutputNode::NdiOutputNode(const std::string& name) : AnimationNode(name) {
    // v3.5.2 Sprint S8 Lot AT — `enabled` port provided by AnimationNode base.

    { ParamDef d; d.name = "source_name"; d.type = ParamType::STRING;
      d.stringVal = "BBFx Output"; d.label = "NDI Source Name"; mSpec.addParam(d); }
    { ParamDef d; d.name = "width"; d.type = ParamType::INT;
      d.intVal = 1920; d.minVal = 320; d.maxVal = 3840; d.label = "Width"; mSpec.addParam(d); }
    { ParamDef d; d.name = "height"; d.type = ParamType::INT;
      d.intVal = 1080; d.minVal = 240; d.maxVal = 2160; d.label = "Height"; mSpec.addParam(d); }
    { ParamDef d; d.name = "fps"; d.type = ParamType::INT;
      d.intVal = 30; d.minVal = 1; d.maxVal = 60; d.label = "FPS Target"; mSpec.addParam(d); }

    setParamSpec(&mSpec);

#ifdef BBFX_HAS_NDI
    if (NDIlib_initialize()) {
        NDIlib_send_create_t desc = {};  // zéro-init : p_groups ne doit pas être garbage
        auto* nameParam = mSpec.getParam("source_name");
        std::string srcName = nameParam ? nameParam->stringVal : "BBFx Output";
        desc.p_ndi_name      = srcName.c_str();
        desc.clock_video     = true;
        desc.clock_audio     = false;
        mNdiSender = NDIlib_send_create(&desc);
        if (mNdiSender) {
            mInitialized = true;
            std::cout << "[NDI] Sender initialized: " << srcName << std::endl;
        } else {
            std::cerr << "[NDI] Failed to create sender" << std::endl;
        }
    } else {
        std::cerr << "[NDI] NDIlib_initialize() failed" << std::endl;
    }
#else
    std::cout << "[NDI] SDK not available — node registered but inactive. "
              << "Install NDI SDK and rebuild with -DBBFX_HAS_NDI=ON" << std::endl;
#endif
}

NdiOutputNode::~NdiOutputNode() {
    destroyPBOs();
#ifdef BBFX_HAS_NDI
    if (mNdiSender) {
        NDIlib_send_destroy(static_cast<NDIlib_send_instance_t>(mNdiSender));
        mNdiSender = nullptr;
    }
    NDIlib_destroy();
#endif
}

void NdiOutputNode::initPBOs(int w, int h) {
#ifdef _WIN32
    loadNdiGL();
    if (!sNdiGLLoaded) return;
    destroyPBOs();
    sNdiGenBuffers(2, mPBO);
    int sz = w * h * 4; // BGRA
    for (int i = 0; i < 2; ++i) {
        sNdiBindBuffer(GL_PIXEL_PACK_BUFFER, mPBO[i]);
        sNdiBufferData(GL_PIXEL_PACK_BUFFER, sz, nullptr, GL_STREAM_READ);
    }
    sNdiBindBuffer(GL_PIXEL_PACK_BUFFER, 0);
    mPBOSize   = sz;
    mPBOWidth  = w;
    mPBOHeight = h;
    mPBOIdx    = 0;
    mPBOReadbacks = 0;   // aucun PBO encore rempli → on saute le 1er send (garbage)
    std::cout << "[NDI] PBOs created: " << w << "x" << h << " (" << sz << " bytes)" << std::endl;
#endif
}

void NdiOutputNode::destroyPBOs() {
#ifdef _WIN32
    if (!sNdiGLLoaded) return;
    if (mPBO[0]) {
        sNdiDelBuffers(2, mPBO);
        mPBO[0] = mPBO[1] = 0;
        mPBOSize = 0;
    }
#endif
}

void NdiOutputNode::sendFrameNDI(const void* data, int w, int h, int fps) {
#ifdef BBFX_HAS_NDI
    if (!mNdiSender || !data) return;
    NDIlib_video_frame_v2_t frame;
    frame.xres              = w;
    frame.yres              = h;
    frame.FourCC            = NDIlib_FourCC_type_BGRA;
    frame.line_stride_in_bytes = w * 4;
    frame.p_data            = static_cast<uint8_t*>(const_cast<void*>(data));
    // Use configured fps: express as rational N/D (e.g. 30 → 30000/1001, 60 → 60000/1001)
    frame.frame_rate_N      = fps * 1000;
    frame.frame_rate_D      = 1001;
    frame.picture_aspect_ratio = static_cast<float>(w) / static_cast<float>(h);
    frame.frame_format_type = NDIlib_frame_format_type_progressive;
    frame.timecode          = NDIlib_send_timecode_synthesize;
    NDIlib_send_send_video_v2(static_cast<NDIlib_send_instance_t>(mNdiSender), &frame);
    ++mSentFrames;
#else
    (void)data; (void)w; (void)h; (void)fps;
#endif
}

void NdiOutputNode::update() {
    if (!mInitialized) return;

    auto& ins = getInputs();
    auto enabledIt = ins.find("enabled");
    float enabled = enabledIt != ins.end() ? enabledIt->second->getValue() : 0.0f;
    if (enabled < 0.5f) { fireUpdate(); return; }

    // Read params
    int w   = mSpec.getParam("width")  ? mSpec.getParam("width")->intVal  : 1920;
    int h   = mSpec.getParam("height") ? mSpec.getParam("height")->intVal : 1080;
    int fps = mSpec.getParam("fps")    ? mSpec.getParam("fps")->intVal    : 30;

    // Rate control: approximate by frame skip
    // Studio runs at ~60 FPS; target = fps, so interval = 60/fps
    if (fps < 1) fps = 1;
    mSendInterval = std::max(1, 60 / fps);

    ++mFrameCount;
    bool sendThisFrame = (mFrameCount % mSendInterval == 0);

#ifdef _WIN32
    // PBO double-buffering async readback
    if (mPBO[0] == 0 || mPBOWidth != w || mPBOHeight != h) {
        initPBOs(w, h);
    }

    if (!sNdiGLLoaded || !mPBO[0]) { fireUpdate(); return; }

    // Read FBO from RenderTexture if accessor available
    if (mGetRenderTexture) {
        auto* rt = mGetRenderTexture();
        if (rt) {
            unsigned int fboId = 0;
            rt->getCustomAttribute("FBO", &fboId);
            if (fboId) {
                // Bind the read framebuffer
                typedef void (APIENTRY* PFNGLBINDFRAMEBUFFERPROC)(GLenum, GLuint);
                static PFNGLBINDFRAMEBUFFERPROC sBindFB = nullptr;
                if (!sBindFB)
                    sBindFB = reinterpret_cast<PFNGLBINDFRAMEBUFFERPROC>(
                        wglGetProcAddress("glBindFramebuffer"));
                if (sBindFB) sBindFB(0x8CA8 /*GL_READ_FRAMEBUFFER*/, fboId);
            }
        }
    }

    if (sendThisFrame) {
        // Map PBO from previous frame (mPBOIdx ^ 1) to get pixels — SAUF tant que
        // le buffer de lecture n'a jamais reçu de glReadPixels (sinon on streamerait
        // de la mémoire GPU non initialisée à la 1ère frame après init/resize).
        if (mPBOReadbacks >= 1) {
            int readPBO = mPBOIdx ^ 1;
            sNdiBindBuffer(GL_PIXEL_PACK_BUFFER, mPBO[readPBO]);
            void* ptr = sNdiMapBuffer(GL_PIXEL_PACK_BUFFER, GL_READ_ONLY);
            if (ptr) {
                sendFrameNDI(ptr, mPBOWidth, mPBOHeight, fps);
                sNdiUnmapBuffer(GL_PIXEL_PACK_BUFFER);
            }
            sNdiBindBuffer(GL_PIXEL_PACK_BUFFER, 0);
        }

        // Start async readback into current PBO.
        sNdiBindBuffer(GL_PIXEL_PACK_BUFFER, mPBO[mPBOIdx]);
        glReadPixels(0, 0, w, h, GL_BGRA, GL_UNSIGNED_BYTE, nullptr);
        sNdiBindBuffer(GL_PIXEL_PACK_BUFFER, 0);

        ++mPBOReadbacks;
        mPBOIdx ^= 1;
    }
#else
    // Non-Windows fallback: direct glReadPixels (blocking, slower)
    if (sendThisFrame) {
        int sz = w * h * 4;
        std::vector<uint8_t> buf(sz);
        glReadPixels(0, 0, w, h, GL_BGRA, GL_UNSIGNED_BYTE, buf.data());
        sendFrameNDI(buf.data(), w, h, fps);
    }
#endif

    fireUpdate();
}

} // namespace bbfx
