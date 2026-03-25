#include "TextureBlitter.h"

namespace bbfx {

TextureBlitter::TextureBlitter(const string& textureName)
    : mTextureName(textureName) {}

TextureBlitter::~TextureBlitter() = default;

void TextureBlitter::blit() {
    // Texture pixel manipulation would happen here
    // Stub: requires active texture in OGRE resource system
}

} // namespace bbfx
