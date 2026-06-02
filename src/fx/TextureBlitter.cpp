#include "TextureBlitter.h"

namespace bbfx {

using namespace Ogre;

TextureBlitter::TextureBlitter(const string& textureName)
    : mTextureName(textureName) {
    mTexture = TextureManager::getSingleton().createManual(
        mTextureName,
        ResourceGroupManager::DEFAULT_RESOURCE_GROUP_NAME,
        TEX_TYPE_2D,
        512, 512, 1, 0,
        PF_X8R8G8B8,
        TU_DYNAMIC_WRITE_ONLY);
}

TextureBlitter::~TextureBlitter() {
    mTexture.reset();
    // Retire la texture manuelle du TextureManager (mTexture.reset() seul ne fait
    // que lâcher la ref locale → la texture reste enregistrée = fuite).
    auto& tm = TextureManager::getSingleton();
    if (!mTextureName.empty() && tm.getByName(mTextureName, ResourceGroupManager::DEFAULT_RESOURCE_GROUP_NAME))
        tm.remove(mTextureName, ResourceGroupManager::DEFAULT_RESOURCE_GROUP_NAME);
}

void TextureBlitter::blit(float r, float g, float b, float a, int pattern) {
    auto buffer = mTexture->getBuffer();
    buffer->lock(HardwareBuffer::HBL_DISCARD);
    const PixelBox& pb = buffer->getCurrentLock();

    size_t height = pb.getHeight();
    size_t width = pb.getWidth();
    const PixelFormat pf = pb.format;
    size_t pixelSize = PixelUtil::getNumElemBytes(pf);
    size_t rowStride = pb.rowPitch * pixelSize; // rowPitch en éléments → octets

    const ColourValue fg(r, g, b, a);
    const ColourValue bg(0, 0, 0, a);
    const size_t cell = 32; // taille de cellule pour checker / stripes

    auto* row = static_cast<unsigned char*>(pb.data);
    for (size_t y = 0; y < height; y++) {
        auto* data = row;
        for (size_t x = 0; x < width; x++) {
            ColourValue c;
            switch (pattern) {
                case 1: { // checker
                    bool on = (((x / cell) + (y / cell)) & 1) == 0;
                    c = on ? fg : bg;
                    break;
                }
                case 2: { // gradient horizontal
                    float t = width > 1 ? float(x) / float(width - 1) : 0.0f;
                    c = ColourValue(r * t, g * t, b * t, a);
                    break;
                }
                case 3: { // gradient vertical
                    float t = height > 1 ? float(y) / float(height - 1) : 0.0f;
                    c = ColourValue(r * t, g * t, b * t, a);
                    break;
                }
                case 4: c = ((y / cell) & 1) ? bg : fg; break; // stripes horizontales
                case 5: c = ((x / cell) & 1) ? bg : fg; break; // stripes verticales
                default: c = fg; break;                        // 0 = solid
            }
            PixelUtil::packColour(c, pf, data);
            data += pixelSize;
        }
        row += rowStride;
    }
    buffer->unlock();
}

} // namespace bbfx
