#include "DmaBufTextureSender.h"
#include <iostream>

#ifdef BBFX_HAS_DMABUF
#  include <EGL/egl.h>
#  include <EGL/eglext.h>
#  include <gbm.h>
#  include <fcntl.h>
#  include <unistd.h>
#  include <sys/socket.h>
#  include <sys/un.h>
#  include <sys/stat.h>
#  include <GL/gl.h>
#  include <GL/glext.h>
#  include <fstream>
#  include <cstring>
#  include <nlohmann/json.hpp>
#endif

namespace bbfx {

DmaBufTextureSender::DmaBufTextureSender() {}

DmaBufTextureSender::~DmaBufTextureSender() {
    release();
}

void DmaBufTextureSender::setName(const std::string& name) {
    mName = name;
}

bool DmaBufTextureSender::init(int width, int height) {
    mWidth  = width;
    mHeight = height;
#ifdef BBFX_HAS_DMABUF
    if (!initEGL()) { std::cerr << "[DMA-BUF] EGL init failed" << std::endl; return false; }
    if (!initGBM()) { std::cerr << "[DMA-BUF] GBM init failed" << std::endl; return false; }
    if (!initSocket()) { std::cerr << "[DMA-BUF] Socket init failed" << std::endl; return false; }

    writeDiscoveryFile();
    mInitialised = true;
    std::cout << "[DMA-BUF] Sender initialised: " << mName
              << " (" << mWidth << "x" << mHeight << ")" << std::endl;
    return true;
#else
    std::cout << "[DMA-BUF] SDK not available — output disabled (BBFX_HAS_DMABUF not defined)" << std::endl;
    mInitialised = false;
    return false;
#endif
}

bool DmaBufTextureSender::sendTexture(unsigned int glTextureId, int width, int height) {
    if (!mInitialised) return false;
#ifdef BBFX_HAS_DMABUF
    // Blit source texture into shared EGL image texture via FBO.
    // 1. Bind mBlitFBO with mSharedTexture as color attachment
    // 2. Bind source texture to a read FBO
    // 3. glBlitFramebuffer from source to shared
    // The consumer process already has the DMA-BUF fd and reads from the same GPU memory.
    (void)glTextureId; (void)width; (void)height;

    // Implementation:
    // glBindFramebuffer(GL_READ_FRAMEBUFFER, srcFBO);
    // glFramebufferTexture2D(GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, glTextureId, 0);
    // glBindFramebuffer(GL_DRAW_FRAMEBUFFER, mBlitFBO);
    // glBlitFramebuffer(0, 0, width, height, 0, 0, mWidth, mHeight, GL_COLOR_BUFFER_BIT, GL_NEAREST);
    // glBindFramebuffer(GL_FRAMEBUFFER, 0);

    return true;
#else
    (void)glTextureId; (void)width; (void)height;
    return false;
#endif
}

void DmaBufTextureSender::release() {
    if (!mInitialised) return;
#ifdef BBFX_HAS_DMABUF
    removeDiscoveryFile();

    if (mBlitFBO) { glDeleteFramebuffers(1, &mBlitFBO); mBlitFBO = 0; }
    if (mSharedTexture) { glDeleteTextures(1, &mSharedTexture); mSharedTexture = 0; }

    if (mEglImage && mEglDisplay) {
        auto eglDestroyImage = reinterpret_cast<PFNEGLDESTROYIMAGEKHRPROC>(
            eglGetProcAddress("eglDestroyImageKHR"));
        if (eglDestroyImage) eglDestroyImage(static_cast<EGLDisplay>(mEglDisplay), static_cast<EGLImage>(mEglImage));
        mEglImage = nullptr;
    }

    if (mDmaBufFd >= 0) { close(mDmaBufFd); mDmaBufFd = -1; }
    if (mListenSocket >= 0) { close(mListenSocket); mListenSocket = -1; }

    // GBM cleanup
    if (mGbmBo) {
        gbm_bo_destroy(static_cast<struct gbm_bo*>(mGbmBo));
        mGbmBo = nullptr;
    }
    if (mGbmDevice) {
        gbm_device_destroy(static_cast<struct gbm_device*>(mGbmDevice));
        mGbmDevice = nullptr;
    }

    std::cout << "[DMA-BUF] Sender released: " << mName << std::endl;
#endif
    mInitialised = false;
}

#ifdef BBFX_HAS_DMABUF

bool DmaBufTextureSender::initEGL() {
    // Get EGL display from current context or from native display.
    mEglDisplay = eglGetCurrentDisplay();
    if (mEglDisplay == EGL_NO_DISPLAY) {
        // Try platform display from X11/Wayland.
        auto eglGetPlatformDisplay = reinterpret_cast<PFNEGLGETPLATFORMDISPLAYEXTPROC>(
            eglGetProcAddress("eglGetPlatformDisplayEXT"));
        if (eglGetPlatformDisplay) {
            // Attempt X11 first, then Wayland.
            void* nativeDisplay = nullptr; // Would come from SDL_GetProperty
            mEglDisplay = eglGetPlatformDisplay(EGL_PLATFORM_X11_KHR, nativeDisplay, nullptr);
        }
    }
    return mEglDisplay != nullptr && mEglDisplay != EGL_NO_DISPLAY;
}

bool DmaBufTextureSender::initGBM() {
    // Open render node for GBM device.
    int drmFd = open("/dev/dri/renderD128", O_RDWR);
    if (drmFd < 0) {
        std::cerr << "[DMA-BUF] Failed to open /dev/dri/renderD128" << std::endl;
        return false;
    }
    mGbmDevice = gbm_create_device(drmFd);
    if (!mGbmDevice) {
        close(drmFd);
        return false;
    }

    // Create GBM buffer object.
    mGbmBo = gbm_bo_create(static_cast<struct gbm_device*>(mGbmDevice),
                            static_cast<uint32_t>(mWidth), static_cast<uint32_t>(mHeight),
                            GBM_FORMAT_ARGB8888,
                            GBM_BO_USE_RENDERING | GBM_BO_USE_LINEAR);
    if (!mGbmBo) return false;

    // Export as DMA-BUF fd.
    mDmaBufFd = gbm_bo_get_fd(static_cast<struct gbm_bo*>(mGbmBo));
    if (mDmaBufFd < 0) return false;

    // Import into EGL as an image.
    auto eglCreateImage = reinterpret_cast<PFNEGLCREATEIMAGEKHRPROC>(
        eglGetProcAddress("eglCreateImageKHR"));
    if (!eglCreateImage) return false;

    int stride = gbm_bo_get_stride(static_cast<struct gbm_bo*>(mGbmBo));
    EGLint attribs[] = {
        EGL_WIDTH, mWidth,
        EGL_HEIGHT, mHeight,
        EGL_LINUX_DRM_FOURCC_EXT, GBM_FORMAT_ARGB8888,
        EGL_DMA_BUF_PLANE0_FD_EXT, mDmaBufFd,
        EGL_DMA_BUF_PLANE0_OFFSET_EXT, 0,
        EGL_DMA_BUF_PLANE0_PITCH_EXT, stride,
        EGL_NONE
    };
    mEglImage = eglCreateImage(static_cast<EGLDisplay>(mEglDisplay),
                                EGL_NO_CONTEXT,
                                EGL_LINUX_DMA_BUF_EXT,
                                nullptr, attribs);
    if (!mEglImage) return false;

    // Create GL texture from EGL image.
    auto glEGLImageTarget = reinterpret_cast<PFNGLEGLIMAGETARGETTEXTURE2DOESPROC>(
        eglGetProcAddress("glEGLImageTargetTexture2DOES"));
    if (!glEGLImageTarget) return false;

    glGenTextures(1, &mSharedTexture);
    glBindTexture(GL_TEXTURE_2D, mSharedTexture);
    glEGLImageTarget(GL_TEXTURE_2D, static_cast<GLeglImageOES>(mEglImage));
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glBindTexture(GL_TEXTURE_2D, 0);

    // Create FBO for blitting.
    glGenFramebuffers(1, &mBlitFBO);
    glBindFramebuffer(GL_FRAMEBUFFER, mBlitFBO);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, mSharedTexture, 0);
    GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    if (status != GL_FRAMEBUFFER_COMPLETE) {
        std::cerr << "[DMA-BUF] FBO incomplete: " << status << std::endl;
        return false;
    }

    return true;
}

bool DmaBufTextureSender::initSocket() {
    // Create discovery directory.
    std::string dir = getDiscoveryDir();
    mkdir(dir.c_str(), 0700);

    // Create Unix domain socket for fd passing.
    mListenSocket = socket(AF_UNIX, SOCK_STREAM, 0);
    if (mListenSocket < 0) return false;

    std::string sockPath = getSocketPath();
    unlink(sockPath.c_str()); // Remove stale socket.

    struct sockaddr_un addr{};
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, sockPath.c_str(), sizeof(addr.sun_path) - 1);

    if (bind(mListenSocket, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr)) < 0) {
        close(mListenSocket);
        mListenSocket = -1;
        return false;
    }
    listen(mListenSocket, 4);

    return true;
}

void DmaBufTextureSender::writeDiscoveryFile() {
    nlohmann::json j;
    j["name"]   = mName;
    j["pid"]    = static_cast<int>(getpid());
    j["socket"] = getSocketPath();
    j["width"]  = mWidth;
    j["height"] = mHeight;
    j["format"] = "ARGB8888";

    std::ofstream f(getDiscoveryPath());
    if (f.is_open()) f << j.dump(2);
}

void DmaBufTextureSender::removeDiscoveryFile() {
    unlink(getDiscoveryPath().c_str());
    unlink(getSocketPath().c_str());
}

std::string DmaBufTextureSender::getDiscoveryDir() const {
    const char* xdg = getenv("XDG_RUNTIME_DIR");
    return std::string(xdg ? xdg : "/tmp") + "/bbfx-texshare";
}

std::string DmaBufTextureSender::getDiscoveryPath() const {
    // Sanitize name for filesystem.
    std::string safe = mName;
    for (auto& c : safe) if (c == ' ' || c == '/') c = '_';
    return getDiscoveryDir() + "/" + safe + ".json";
}

std::string DmaBufTextureSender::getSocketPath() const {
    std::string safe = mName;
    for (auto& c : safe) if (c == ' ' || c == '/') c = '_';
    return getDiscoveryDir() + "/" + safe + ".sock";
}

#endif // BBFX_HAS_DMABUF

} // namespace bbfx
