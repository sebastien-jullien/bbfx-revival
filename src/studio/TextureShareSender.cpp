#include "TextureShareSender.h"
#include <iostream>

// Include concrete backends based on compile flags.
#ifdef BBFX_HAS_SPOUT
#  include "SpoutTextureSender.h"
#endif
#ifdef BBFX_HAS_DMABUF
#  include "DmaBufTextureSender.h"
#endif

namespace bbfx {

// ── NullTextureSender — no-op fallback ──────────────────────────────────────

class NullTextureSender : public TextureShareSender {
public:
    void setName(const std::string& name) override { mName = name; }

    bool init(int /*width*/, int /*height*/) override {
        std::cout << "[TextureShare] No backend available — texture sharing disabled." << std::endl;
        return false;
    }

    bool sendTexture(unsigned int /*glTextureId*/, int /*width*/, int /*height*/) override {
        return false;
    }

    void release() override {}

    bool isInitialised() const override { return false; }
    const std::string& getName() const override { return mName; }
    const char* backendName() const override { return "None"; }

private:
    std::string mName = "BBFx Output";
};

// ── Factory ─────────────────────────────────────────────────────────────────

std::unique_ptr<TextureShareSender> createTextureSender() {
#if defined(BBFX_HAS_SPOUT)
    return std::make_unique<SpoutTextureSender>();
#elif defined(BBFX_HAS_DMABUF)
    return std::make_unique<DmaBufTextureSender>();
#else
    return std::make_unique<NullTextureSender>();
#endif
}

const char* getTextureShareLabel() {
#if defined(BBFX_HAS_SPOUT)
    return "Spout";
#elif defined(BBFX_HAS_DMABUF)
    return "DMA-BUF";
#else
    return "Texture Sharing";
#endif
}

bool isTextureShareAvailable() {
#if defined(BBFX_HAS_SPOUT) || defined(BBFX_HAS_DMABUF)
    return true;
#else
    return false;
#endif
}

} // namespace bbfx
