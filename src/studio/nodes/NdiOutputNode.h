#pragma once
#include "../../core/AnimationNode.h"
#include "../../core/ParamSpec.h"
#include <functional>
#include <string>

namespace Ogre { class RenderTexture; }

namespace bbfx {

/// DAG node that sends video frames via NDI (NewTek Network Device Interface).
///
/// When BBFX_HAS_NDI is not defined, the node registers but does nothing (graceful no-op).
///
/// Features (v3.4 Lot I):
///   - PBO double-buffering for async readback (< 1ms frame-time overhead)
///   - BGRA format (NDI-native, no conversion)
///   - FPS rate control (send every N frames)
///   - Source name configurable via ParamSpec
class NdiOutputNode : public AnimationNode {
public:
    explicit NdiOutputNode(const std::string& name);
    ~NdiOutputNode() override;
    void update() override;
    std::string getTypeName() const override { return "NdiOutputNode"; }

    /// Provide access to the OGRE RenderTexture for GL readback.
    void setRenderTextureAccessor(std::function<Ogre::RenderTexture*()> fn) {
        mGetRenderTexture = std::move(fn);
    }

private:
    void initPBOs(int w, int h);
    void destroyPBOs();
    void sendFrameNDI(const void* data, int w, int h, int fps = 30);

    ParamSpec mSpec;
    bool mInitialized  = false;
    void* mNdiSender   = nullptr; // NDIlib_send_instance_t

    // PBO double-buffering (async readback)
    unsigned int mPBO[2]  = {0, 0};
    int          mPBOIdx  = 0;      // index of PBO being read this frame
    int          mPBOReadbacks = 0; // nb de readbacks émis depuis initPBOs (anti 1ère frame garbage)
    int          mPBOSize = 0;      // allocated size in bytes
    int          mPBOWidth  = 0;
    int          mPBOHeight = 0;

    // Rate control
    uint32_t mFrameCount   = 0;
    int      mSendInterval = 1; // send every N frames

    // NDI metadata
    int mSentFrames = 0;

    std::function<Ogre::RenderTexture*()> mGetRenderTexture;
};

} // namespace bbfx
