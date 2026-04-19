#include "TextureShareReceiver.h"

#include <iostream>

#ifdef BBFX_HAS_SPOUT
#  include "SpoutTextureReceiver.h"
#endif
#ifdef BBFX_HAS_DMABUF
#  include "DmaBufTextureReceiver.h"
#endif

namespace bbfx {

// ── NullTextureReceiver — compile-time fallback -----------------------------

class NullTextureReceiver : public TextureShareReceiver {
public:
    bool init(const std::string& sourceName) override {
        mSourceName = sourceName;
        std::cout << "[TextureShare] No receiver backend available — "
                     "init(\"" << sourceName << "\") returning false." << std::endl;
        return false;
    }
    bool updateTexture() override { return false; }
    std::string getTextureName() const override { return mTextureName; }
    void release() override { mTextureName.clear(); }
    const std::string& getSourceName() const override { return mSourceName; }
    const char* backendName() const override { return "Null"; }

private:
    std::string mSourceName;
    std::string mTextureName;
};

// ── Factory -----------------------------------------------------------------

std::unique_ptr<TextureShareReceiver> createTextureReceiver(
    const std::string& sourceName)
{
#ifdef BBFX_HAS_SPOUT
    auto r = std::make_unique<SpoutTextureReceiver>();
    r->init(sourceName);
    return r;
#elif defined(BBFX_HAS_DMABUF)
    auto r = std::make_unique<DmaBufTextureReceiver>();
    r->init(sourceName);
    return r;
#else
    auto r = std::make_unique<NullTextureReceiver>();
    r->init(sourceName);
    return r;
#endif
}

// ── Static helpers ----------------------------------------------------------

std::vector<std::string> TextureShareReceiver::listAvailableSources() {
#ifdef BBFX_HAS_SPOUT
    return SpoutTextureReceiver::enumerateSources();
#elif defined(BBFX_HAS_DMABUF)
    return DmaBufTextureReceiver::enumerateSources();
#else
    return {};
#endif
}

const char* TextureShareReceiver::platformBackend() {
#ifdef BBFX_HAS_SPOUT
    return "Spout";
#elif defined(BBFX_HAS_DMABUF)
    return "DMA-BUF";
#else
    return "Null";
#endif
}

} // namespace bbfx
