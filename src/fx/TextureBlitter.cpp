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
}

void TextureBlitter::blit() {
    auto buffer = mTexture->getBuffer();
    buffer->lock(HardwareBuffer::HBL_DISCARD);
    const PixelBox& pb = buffer->getCurrentLock();

    size_t height = pb.getHeight();
    size_t width = pb.getWidth();
    size_t rowSkip = pb.getRowSkip();
    const PixelFormat pf = pb.format;
    size_t pixelSize = PixelUtil::getNumElemBytes(pf);

    ColourValue colour = ColourValue::Blue;
    auto* data = static_cast<unsigned char*>(pb.data);
    for (size_t y = 0; y < height; y++) {
        for (size_t x = 0; x < width; x++) {
            PixelUtil::packColour(colour, pf, data);
            data += pixelSize;
        }
        data += rowSkip;
    }
    buffer->unlock();
}

} // namespace bbfx
