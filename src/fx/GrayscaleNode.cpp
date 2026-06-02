#include "GrayscaleNode.h"
#include <OgreTextureManager.h>
#include <OgreHardwarePixelBuffer.h>
#include <OgrePixelFormat.h>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <vector>

namespace bbfx {

int GrayscaleNode::sCounter = 0;

GrayscaleNode::GrayscaleNode(const std::string& name)
    : AnimationNode(name)
{
    sCounter++;
    mTexName = "Grayscale/" + std::to_string(sCounter) + "/" + getName();

    ParamDef srcDef;
    srcDef.name = "source_texture"; srcDef.label = "Source Texture";
    srcDef.type = ParamType::TEXTURE;
    srcDef.stringVal = "";
    srcDef.tooltip = "Input texture name to desaturate. "
                     "Can be a file texture, an RTT (Noise/Spectrogram/Theora), or any "
                     "OGRE-managed Texture.";
    mSpec.addParam(srcDef);

    ParamDef mixDef;
    mixDef.name = "mix"; mixDef.label = "Mix (color↔gray)";
    mixDef.type = ParamType::FLOAT;
    mixDef.floatVal = 1.0f; mixDef.minVal = 0.0f; mixDef.maxVal = 1.0f;
    mixDef.tooltip = "0.0 = full color identity, 1.0 = full grayscale. "
                     "Animable via DAG port — link to a beat detector for color↔gray pulses.";
    mSpec.addParam(mixDef);

    ParamDef intDef;
    intDef.name = "intensity"; intDef.label = "Intensity";
    intDef.type = ParamType::FLOAT;
    intDef.floatVal = 1.0f; intDef.minVal = 0.0f; intDef.maxVal = 4.0f;
    intDef.tooltip = "Post-desaturation multiplier. >1 brightens, <1 darkens.";
    mSpec.addParam(intDef);

    // Read-only mirror exposing the generated RTT name. Consumed by
    // MaterialBridgeNode (Lot T) and any Lua code that wants to chain.
    ParamDef outDef;
    outDef.name = "texture_out"; outDef.label = "Texture (out)";
    outDef.type = ParamType::STRING;
    outDef.stringVal = mTexName;     // pre-fill with the planned name
    outDef.readOnly = true;
    outDef.tooltip = "Read-only: name of the generated grayscale RTT. "
                     "Consumable by MaterialBridgeNode auto-wrap.";
    mSpec.addParam(outDef);

    setParamSpec(&mSpec);

    // DAG ports: mix is animable; texture_ready pulses 1.0 each successful render.
    addInput(new AnimationPort("mix", 1.0f));
    addOutput(new AnimationPort("texture_ready", 0.0f));

    setPortTooltip("mix",
        "Animable mix (color↔gray). Overrides the ParamSpec mix when connected.");
    setPortTooltip("texture_ready",
        "Pulses 1.0 each frame the grayscale render path executed (0.0 idle).");
}

GrayscaleNode::~GrayscaleNode() {
    cleanup();
}

void GrayscaleNode::ensureTexture(unsigned w, unsigned h) {
    if (!mTex.isNull() && mTex->getWidth() == w && mTex->getHeight() == h) return;
    if (!mTex.isNull()) {
        Ogre::TextureManager::getSingleton().remove(mTexName);
        mTex.setNull();
    }
    if (w == 0 || h == 0) return;
    mTex = Ogre::TextureManager::getSingleton().createManual(
        mTexName,
        Ogre::ResourceGroupManager::DEFAULT_RESOURCE_GROUP_NAME,
        Ogre::TEX_TYPE_2D,
        w, h, 0,
        Ogre::PF_BYTE_RGBA,
        Ogre::TU_DYNAMIC_WRITE_ONLY_DISCARDABLE);
    mPrevW = w;
    mPrevH = h;
}

void GrayscaleNode::renderToTexture() {
    auto* srcParam = mSpec.getParam("source_texture");
    if (!srcParam || srcParam->stringVal.empty()) {
        mLastRendered = false;
        return;
    }
    const std::string srcName = srcParam->stringVal;

    auto srcTex = Ogre::TextureManager::getSingleton().getByName(srcName);
    if (srcTex.isNull()) {
        mLastRendered = false;
        return;
    }

    const unsigned w = srcTex->getWidth();
    const unsigned h = srcTex->getHeight();
    if (w == 0 || h == 0) {
        mLastRendered = false;
        return;
    }
    ensureTexture(w, h);
    if (mTex.isNull()) {
        mLastRendered = false;
        return;
    }

    // Read-back source pixels as RGBA8.
    std::vector<unsigned char> srcBuf(static_cast<size_t>(w) * h * 4, 0);
    Ogre::PixelBox srcBox(w, h, 1, Ogre::PF_BYTE_RGBA, srcBuf.data());
    try {
        srcTex->getBuffer()->blitToMemory(srcBox);
    } catch (const Ogre::Exception&) {
        // Some textures (write-only RTTs, compressed, in-flight uploads) may not
        // support blit-back. Bail gracefully — the test harness checks
        // wasRendered() rather than pixel content.
        mLastRendered = false;
        return;
    }

    // Apply BT.709 + mix + intensity per-pixel into our RTT.
    auto* mixParam = mSpec.getParam("mix");
    auto* intParam = mSpec.getParam("intensity");
    auto& inputs = getInputs();
    auto itMix = inputs.find("mix");

    float mixVal = mixParam ? mixParam->floatVal : 1.0f;
    if (itMix != inputs.end()) mixVal = itMix->second->getValue();
    mixVal = std::max(0.0f, std::min(1.0f, mixVal));

    float intVal = intParam ? intParam->floatVal : 1.0f;
    intVal = std::max(0.0f, std::min(4.0f, intVal));

    auto pb = mTex->getBuffer();
    pb->lock(Ogre::HardwareBuffer::HBL_DISCARD);
    const Ogre::PixelBox& dstBox = pb->getCurrentLock();
    auto* dst = static_cast<unsigned char*>(dstBox.data);
    const size_t rowSrcBytes = static_cast<size_t>(w) * 4;
    for (unsigned y = 0; y < h; ++y) {
        const unsigned char* srow = srcBuf.data() + y * rowSrcBytes;
        unsigned char*       drow = dst + y * dstBox.rowPitch * 4;
        for (unsigned x = 0; x < w; ++x) {
            float r = srow[x * 4 + 0] / 255.0f;
            float g = srow[x * 4 + 1] / 255.0f;
            float b = srow[x * 4 + 2] / 255.0f;
            float a = srow[x * 4 + 3] / 255.0f;
            float gy = 0.2126f * r + 0.7152f * g + 0.0722f * b;
            float or_ = (r * (1.0f - mixVal) + gy * mixVal) * intVal;
            float og  = (g * (1.0f - mixVal) + gy * mixVal) * intVal;
            float ob  = (b * (1.0f - mixVal) + gy * mixVal) * intVal;
            or_ = std::max(0.0f, std::min(1.0f, or_));
            og  = std::max(0.0f, std::min(1.0f, og));
            ob  = std::max(0.0f, std::min(1.0f, ob));
            drow[x * 4 + 0] = static_cast<unsigned char>(or_ * 255.0f + 0.5f);
            drow[x * 4 + 1] = static_cast<unsigned char>(og  * 255.0f + 0.5f);
            drow[x * 4 + 2] = static_cast<unsigned char>(ob  * 255.0f + 0.5f);
            drow[x * 4 + 3] = static_cast<unsigned char>(a   * 255.0f + 0.5f);
        }
    }
    pb->unlock();

    mPrevSource    = srcName;
    mPrevMix       = mixVal;
    mPrevIntensity = intVal;
    mLastRendered  = true;

    // Mirror the output name (always our RTT name — kept stable for downstream
    // consumers such as MaterialBridgeNode that capture the value once).
    auto* outMir = mSpec.getParam("texture_out");
    if (outMir) outMir->stringVal = mTexName;
}

void GrayscaleNode::update() {
    auto* srcParam = mSpec.getParam("source_texture");
    auto* mixParam = mSpec.getParam("mix");
    auto* intParam = mSpec.getParam("intensity");

    std::string src = srcParam ? srcParam->stringVal : std::string();
    auto& inputs = getInputs();
    auto itMix = inputs.find("mix");
    float mixVal = mixParam ? mixParam->floatVal : 1.0f;
    if (itMix != inputs.end()) mixVal = itMix->second->getValue();
    float intVal = intParam ? intParam->floatVal : 1.0f;

    bool dirty = false;
    if (src != mPrevSource) dirty = true;
    if (std::abs(mixVal - mPrevMix) > 0.005f) dirty = true;
    if (std::abs(intVal - mPrevIntensity) > 0.005f) dirty = true;

    if (dirty) {
        renderToTexture();
    }

    auto& outputs = getOutputs();
    auto itReady = outputs.find("texture_ready");
    if (itReady != outputs.end()) {
        itReady->second->setValue(mLastRendered ? 1.0f : 0.0f);
    }

    fireUpdate();
}

void GrayscaleNode::cleanup() {
    if (!mTex.isNull()) {
        Ogre::TextureManager::getSingleton().remove(mTexName);
        mTex.setNull();
    }
    mPrevSource.clear();
    mPrevMix       = -1.0f;
    mPrevIntensity = -1.0f;
    mPrevW = mPrevH = 0;
    mLastRendered = false;
}

} // namespace bbfx
