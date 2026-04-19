#pragma once

#include <memory>
#include <string>
#include <vector>

namespace bbfx {

/// v3.5 Lot M — symmetric counterpart of TextureShareSender.
///
/// Pulls a GPU texture published by another app (OBS Spout source,
/// Resolume, TouchDesigner, another BBFx instance) and exposes it to
/// BBFx as an OGRE texture name.
///
/// Backends:
///   - SpoutTextureReceiver  (Windows, BBFX_HAS_SPOUT)
///   - DmaBufTextureReceiver (Linux,   BBFX_HAS_DMABUF)  — stub
///   - NullTextureReceiver   (fallback) — no-op, returns false from
///     init/updateTexture so callers can fail gracefully.
///
/// Use createTextureReceiver() to instantiate the right backend.
class TextureShareReceiver {
public:
    virtual ~TextureShareReceiver() = default;

    /// Bind this receiver to a named source. May be called before the
    /// producer exists — updateTexture() will then return false until a
    /// frame arrives.
    virtual bool init(const std::string& sourceName) = 0;

    /// Pull a fresh frame (if available) into the internal OGRE texture.
    /// Returns true iff a new frame was received this call.
    virtual bool updateTexture() = 0;

    /// OGRE texture name that callers can assign to a material's
    /// texture_unit. Stable across frames once init() succeeded. Empty
    /// string if the receiver has not been initialised.
    virtual std::string getTextureName() const = 0;

    /// Release the receiver and free resources.
    virtual void release() = 0;

    /// Source name passed to init().
    virtual const std::string& getSourceName() const = 0;

    /// Human-readable backend label (same convention as the sender).
    virtual const char* backendName() const = 0;

    // ── static helpers ──────────────────────────────────────────────────

    /// All currently-advertised texture sharing sources on the system.
    /// Returns an empty list on platforms without SDK support.
    static std::vector<std::string> listAvailableSources();

    /// "Spout", "DMA-BUF", or "Null" depending on compile flags.
    static const char* platformBackend();
};

/// Factory — picks the best backend available at compile time.
std::unique_ptr<TextureShareReceiver> createTextureReceiver(
    const std::string& sourceName);

} // namespace bbfx
