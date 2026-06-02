#include "TextureShareOutputNode.h"
#include "../../core/AnimationPort.h"
#include <iostream>

namespace bbfx {

TextureShareOutputNode::TextureShareOutputNode(const std::string& name)
    : AnimationNode(name) {
    // ParamSpec
    { ParamDef d; d.name = "source_name"; d.label = "Source Name"; d.type = ParamType::STRING;
      d.stringVal = "BBFx Output"; mSpec.addParam(d); }
    { ParamDef d; d.name = "width"; d.label = "Width"; d.type = ParamType::INT;
      d.intVal = 1920; d.minVal = 64.f; d.maxVal = 7680.f; mSpec.addParam(d); }
    { ParamDef d; d.name = "height"; d.label = "Height"; d.type = ParamType::INT;
      d.intVal = 1080; d.minVal = 64.f; d.maxVal = 4320.f; mSpec.addParam(d); }
    setParamSpec(&mSpec);

    // v3.5.2 Sprint S8 Lot AT — `enabled` port provided by AnimationNode base
    // (default 1.0). This output node is off-by-default: override to 0.0 so it
    // doesn't broadcast until the user enables it.
    if (auto it = getInputs().find("enabled"); it != getInputs().end()) {
        it->second->setValue(0.0f);
    }
    setEnabled(false);   // node frozen until enabled

    // Create platform-appropriate sender via factory.
    mSender = createTextureSender();
}

TextureShareOutputNode::~TextureShareOutputNode() {
    if (mSender) mSender->release();
}

void TextureShareOutputNode::update() {
    float enabled = 0.f;
    auto& ins = getInputs();
    auto it = ins.find("enabled");
    if (it != ins.end()) enabled = static_cast<float>(it->second->getValue());

    bool shouldEnable = enabled >= 0.5f;

    // Sync name from ParamSpec
    auto* pName = mSpec.getParam("source_name");
    auto* pW    = mSpec.getParam("width");
    auto* pH    = mSpec.getParam("height");
    std::string srcName = pName ? pName->stringVal : "BBFx Output";
    int w = pW ? pW->intVal : 1920;
    int h = pH ? pH->intVal : 1080;

    if (shouldEnable && !mWasEnabled) {
        if (mSender) {
            mSender->setName(srcName);
            bool ok = mSender->init(w, h);
            if (!ok)
                std::cout << "[TextureShareOutputNode] " << mSender->backendName()
                          << " backend not available — output disabled." << std::endl;
            mWasEnabled = ok;
        }
    } else if (!shouldEnable && mWasEnabled) {
        if (mSender) mSender->release();
        mWasEnabled = false;
    }

    // Send texture each frame when enabled
    if (shouldEnable && mSender && mSender->isInitialised() && mGetTexture) {
        unsigned int texId = mGetTexture();
        if (texId && !mSender->sendTexture(texId, w, h)) {
            std::cout << "[TextureShareOutputNode] sendTexture failed" << std::endl;
        }
    }

    fireUpdate();
}

void TextureShareOutputNode::cleanup() {
    if (mSender) mSender->release();
    mWasEnabled = false;
}

} // namespace bbfx
