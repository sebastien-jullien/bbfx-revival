#pragma once

#include "../../core/AnimationNode.h"
#include "../../core/ParamSpec.h"
#include "../TextureShareSender.h"
#include <string>
#include <functional>

namespace bbfx {

/// DAG node that sends its connected render texture via the platform texture sharing backend.
///
/// On Windows: uses Spout (if BBFX_HAS_SPOUT). On Linux: uses DMA-BUF (if BBFX_HAS_DMABUF).
/// When no backend is available, all operations are no-ops.
///
/// ParamSpec parameters:
///   source_name (string) — source name visible to consumers
///   width  (int)         — texture width
///   height (int)         — texture height
///
/// Input port "enabled" (float, threshold 0.5): if >= 0.5, send frames.
class TextureShareOutputNode : public AnimationNode {
public:
    explicit TextureShareOutputNode(const std::string& name = "");
    ~TextureShareOutputNode() override;

    std::string getTypeName() const override { return "TextureShareOutputNode"; }
    void update() override;
    void cleanup() override;

    /// Called by OutputManager/StudioApp to provide the GL texture to send.
    void setTextureCallback(std::function<unsigned int()> fn) { mGetTexture = std::move(fn); }

private:
    ParamSpec  mSpec;
    std::unique_ptr<TextureShareSender> mSender;
    std::function<unsigned int()> mGetTexture;
    bool mWasEnabled = false;
};

} // namespace bbfx
