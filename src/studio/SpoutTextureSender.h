#pragma once

#include "TextureShareSender.h"

namespace bbfx {

/// Spout backend for TextureShareSender (Windows-only, zero-copy GPU texture sharing).
///
/// When BBFX_HAS_SPOUT is not defined, init() returns false and all operations are no-ops.
/// When BBFX_HAS_SPOUT is defined, the Spout SDK is used for GPU-to-GPU sharing.
class SpoutTextureSender : public TextureShareSender {
public:
    SpoutTextureSender();
    ~SpoutTextureSender() override;

    void setName(const std::string& name) override;
    bool init(int width, int height) override;
    bool sendTexture(unsigned int glTextureId, int width, int height) override;
    void release() override;
    bool isInitialised() const override { return mInitialised; }
    const std::string& getName() const override { return mName; }
    const char* backendName() const override { return "Spout"; }

private:
    std::string mName     = "BBFx Output";
    bool        mInitialised = false;
    int         mWidth    = 0;
    int         mHeight   = 0;

#ifdef BBFX_HAS_SPOUT
    void* mSender = nullptr;
#endif
};

} // namespace bbfx
