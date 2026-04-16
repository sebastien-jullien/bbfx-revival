#pragma once

#include "TextureShareSender.h"

namespace bbfx {

/// DMA-BUF + EGL backend for TextureShareSender (Linux-only, zero-copy GPU texture sharing).
///
/// When BBFX_HAS_DMABUF is not defined, init() returns false and all operations are no-ops.
/// When BBFX_HAS_DMABUF is defined, uses EGL images backed by GBM buffers, exported as
/// DMA-BUF file descriptors and shared with consumers via Unix domain sockets.
///
/// Discovery: writes metadata to $XDG_RUNTIME_DIR/bbfx-texshare/<name>.json
/// so consumers can find and connect to this sender.
class DmaBufTextureSender : public TextureShareSender {
public:
    DmaBufTextureSender();
    ~DmaBufTextureSender() override;

    void setName(const std::string& name) override;
    bool init(int width, int height) override;
    bool sendTexture(unsigned int glTextureId, int width, int height) override;
    void release() override;
    bool isInitialised() const override { return mInitialised; }
    const std::string& getName() const override { return mName; }
    const char* backendName() const override { return "DMA-BUF"; }

private:
    std::string mName     = "BBFx Output";
    bool        mInitialised = false;
    int         mWidth    = 0;
    int         mHeight   = 0;

#ifdef BBFX_HAS_DMABUF
    // EGL / GBM / socket resources (Linux-only)
    void* mEglDisplay  = nullptr;  // EGLDisplay
    void* mEglImage    = nullptr;  // EGLImage
    void* mGbmDevice   = nullptr;  // struct gbm_device*
    void* mGbmBo       = nullptr;  // struct gbm_bo*
    int   mDmaBufFd    = -1;       // exported DMA-BUF file descriptor
    int   mListenSocket = -1;      // Unix domain socket for fd passing
    unsigned int mSharedTexture = 0; // GL texture backed by EGL image
    unsigned int mBlitFBO = 0;       // FBO for blit from source to shared texture

    bool initEGL();
    bool initGBM();
    bool initSocket();
    void writeDiscoveryFile();
    void removeDiscoveryFile();
    std::string getDiscoveryDir() const;
    std::string getDiscoveryPath() const;
    std::string getSocketPath() const;
#endif
};

} // namespace bbfx
