#pragma once

#include <string>
#include <memory>

namespace bbfx {

/// Abstract interface for cross-platform GPU texture sharing.
///
/// Backends:
///   - SpoutTextureSender  (Windows, BBFX_HAS_SPOUT)  — Spout SDK zero-copy GPU sharing
///   - DmaBufTextureSender (Linux, BBFX_HAS_DMABUF)   — DMA-BUF + EGL zero-copy GPU sharing
///   - NullTextureSender   (fallback)                  — No-op when no SDK available
///
/// The API mirrors the original SpoutSender interface for backward compatibility.
/// Use createTextureSender() factory to get the right backend for the current platform.
class TextureShareSender {
public:
    virtual ~TextureShareSender() = default;

    /// Set the source name (must be called before init()).
    virtual void setName(const std::string& name) = 0;

    /// Initialise the sender. Returns true on success.
    virtual bool init(int width, int height) = 0;

    /// Send a GL texture to consumers. Returns true on success.
    virtual bool sendTexture(unsigned int glTextureId, int width, int height) = 0;

    /// Release the sender and free resources.
    virtual void release() = 0;

    /// Query whether the sender is currently initialised and ready to send.
    virtual bool isInitialised() const = 0;

    /// Get the source name.
    virtual const std::string& getName() const = 0;

    /// Human-readable backend name for UI display ("Spout", "DMA-BUF", "None").
    virtual const char* backendName() const = 0;
};

/// Factory: returns the best available texture sharing backend for the current platform.
/// - Windows + BBFX_HAS_SPOUT  → SpoutTextureSender
/// - Linux + BBFX_HAS_DMABUF   → DmaBufTextureSender
/// - Otherwise                  → NullTextureSender (all methods return false)
std::unique_ptr<TextureShareSender> createTextureSender();

/// Returns the platform-appropriate UI label for the texture sharing section.
/// "Spout" on Windows (if available), "DMA-BUF" on Linux (if available), "Texture Sharing" otherwise.
const char* getTextureShareLabel();

/// Returns true if a texture sharing backend is available at compile time.
bool isTextureShareAvailable();

} // namespace bbfx
