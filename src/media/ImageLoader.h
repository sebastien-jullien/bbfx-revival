#pragma once

#include <string>

namespace bbfx {

/// v3.5 Lot Q — static image loader backed by OGRE's TextureManager.
///
/// Supports every image format OGRE codecs handle natively: PNG, JPG,
/// BMP, TGA, HDR, DDS. The input can be either an OGRE resource name
/// (that lives in one of the existing resource groups) or an absolute
/// filesystem path — in the second case the file's parent directory is
/// added as a temporary "BBFx_ImageLoader" resource location so the
/// texture can be loaded without editing resources.cfg.
class ImageLoader {
public:
    /// Load an image and return the OGRE texture name. Returns an empty
    /// string on failure. The texture is cached under its canonical name
    /// so a subsequent call with the same `source` is a no-op.
    static std::string load(const std::string& source);

    /// Release the OGRE texture. Safe to call on an unknown name.
    static void release(const std::string& textureName);
};

} // namespace bbfx
