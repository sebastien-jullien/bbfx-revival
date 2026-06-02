#include "TextureCycleNode.h"
#include "../../core/PrimitiveNodes.h"
#include <algorithm>
#include <chrono>
#include <iostream>
#include <sstream>

namespace bbfx {


TextureCycleNode::TextureCycleNode(const std::string& name)
    : AnimationNode(name)
{
    // ParamSpec
    ParamDef texturesDef;
    texturesDef.name = "textures"; texturesDef.label = "Textures (CSV)";
    texturesDef.type = ParamType::STRING;
    texturesDef.stringVal = "";  // semicolon-separated names
    mSpec.addParam(texturesDef);

    ParamDef modeDef;
    modeDef.name = "mode"; modeDef.label = "Mode"; modeDef.type = ParamType::ENUM;
    modeDef.stringVal = "sequential";
    modeDef.choices = {"sequential", "random", "bpm_synced", "audio_triggered"};
    mSpec.addParam(modeDef);

    ParamDef tDef;
    tDef.name = "transition_time"; tDef.label = "Transition Time (s)";
    tDef.type = ParamType::FLOAT;
    tDef.floatVal = 1.0f; tDef.minVal = 0.0f; tDef.maxVal = 10.0f;
    mSpec.addParam(tDef);

    ParamDef bpmDef;
    bpmDef.name = "auto_advance_bpm"; bpmDef.label = "Auto-Advance (xBPM)";
    bpmDef.type = ParamType::FLOAT;
    bpmDef.floatVal = 0.0f; bpmDef.minVal = 0.0f; bpmDef.maxVal = 4.0f;
    mSpec.addParam(bpmDef);

    ParamDef seedDef;
    seedDef.name = "random_seed"; seedDef.label = "Random Seed";
    seedDef.type = ParamType::INT;
    seedDef.intVal = 0; seedDef.minVal = 0; seedDef.maxVal = 1'000'000;
    mSpec.addParam(seedDef);

    // Read-only ParamSpec mirrors of current/next texture names (Inspector display).
    ParamDef curTexDef;
    curTexDef.name = "current_texture"; curTexDef.label = "Current Texture";
    curTexDef.type = ParamType::STRING; curTexDef.stringVal = "";
    curTexDef.readOnly = true;
    curTexDef.tooltip = "Read-only: name of the texture currently displayed in the cycle.";
    mSpec.addParam(curTexDef);

    ParamDef nxtTexDef;
    nxtTexDef.name = "next_texture"; nxtTexDef.label = "Next Texture";
    nxtTexDef.type = ParamType::STRING; nxtTexDef.stringVal = "";
    nxtTexDef.readOnly = true;
    nxtTexDef.tooltip = "Read-only: name of the upcoming texture (target of next transition).";
    mSpec.addParam(nxtTexDef);

    setParamSpec(&mSpec);

    // Input ports
    addInput(new AnimationPort("dt", 1.0f / 60.0f));
    addInput(new AnimationPort("index", 0.0f));
    addInput(new AnimationPort("next", 0.0f));        // trigger
    addInput(new AnimationPort("prev", 0.0f));        // trigger
    addInput(new AnimationPort("goto_index", -1.0f)); // -1 = no goto request
    addInput(new AnimationPort("beat", 0.0f));        // for AudioTriggered mode

    // Output ports — index-encoded (DAG ports are float-only).
    addOutput(new AnimationPort("current_index", 0.0f));
    addOutput(new AnimationPort("next_index", 0.0f));
    addOutput(new AnimationPort("transition_progress", 0.0f));
    // v3.5.2 Sprint S7 Lot AA — texture_ready : 1.0 pendant une transition active.
    addOutput(new AnimationPort("texture_ready", 0.0f));

    // v3.5.2 Sprint S7 Lot Z — port tooltips
    setPortTooltip("index",                "Direct integer index of the current texture (overrides next/prev when changed).");
    setPortTooltip("next",                 "Trigger (rising edge) advances to the next texture using transition_time.");
    setPortTooltip("prev",                 "Trigger (rising edge) goes back to the previous texture.");
    setPortTooltip("goto_index",           "Floored integer target. -1 = no request, otherwise jumps and starts a transition.");
    setPortTooltip("beat",                 "Audio-triggered mode only: each rising edge requests a next() if cooldown elapsed.");
    setPortTooltip("current_index",        "Active index (output). Useful to drive other cycles in lock-step.");
    setPortTooltip("next_index",           "Index that we are transitioning to (during fade); equals current_index when idle.");
    setPortTooltip("transition_progress",  "Fade progress 0..1 (0 = idle on current_index, 1 = arrived on next_index).");
    setPortTooltip("texture_ready",        "Pulses 1.0 while a transition is active, 0.0 idle (drives border-pulse green).");
}

void TextureCycleNode::setTextures(const std::vector<std::string>& list) {
    mTextures = list;
    if (mCurrentIndex >= static_cast<int>(mTextures.size())) mCurrentIndex = 0;
    if (mNextIndex    >= static_cast<int>(mTextures.size())) mNextIndex = mCurrentIndex;

    // Mirror the list back into ParamSpec (semicolon-separated for serialization).
    std::ostringstream oss;
    for (size_t i = 0; i < list.size(); ++i) {
        if (i) oss << ';';
        oss << list[i];
    }
    auto* p = mSpec.getParam("textures");
    if (p) p->stringVal = oss.str();
}

const std::string& TextureCycleNode::getCurrentTexture() const {
    static const std::string empty;
    if (mTextures.empty() || mCurrentIndex < 0
     || mCurrentIndex >= static_cast<int>(mTextures.size())) return empty;
    return mTextures[mCurrentIndex];
}

const std::string& TextureCycleNode::getNextTexture() const {
    static const std::string empty;
    if (mTextures.empty() || mNextIndex < 0
     || mNextIndex >= static_cast<int>(mTextures.size())) return empty;
    return mTextures[mNextIndex];
}

void TextureCycleNode::seedRngIfNeeded() {
    if (mRngInitialized) return;
    auto* sp = mSpec.getParam("random_seed");
    int seed = (sp ? sp->intVal : 0);
    if (seed > 0) {
        mRng.seed(static_cast<uint32_t>(seed));
    } else {
        auto t = std::chrono::high_resolution_clock::now().time_since_epoch().count();
        mRng.seed(static_cast<uint32_t>(t) ^ static_cast<uint32_t>(reinterpret_cast<uintptr_t>(this)));
    }
    mRandomSeed = seed;
    mRngInitialized = true;
}

int TextureCycleNode::pickRandomNext() {
    seedRngIfNeeded();
    int n = static_cast<int>(mTextures.size());
    if (n <= 1) return 0;
    std::uniform_int_distribution<int> dist(0, n - 2);
    int pick = dist(mRng);
    if (pick >= mCurrentIndex) pick++;  // skip the current to avoid consecutive repetition
    return pick;
}

// If a fade is already in flight, snap to its target before starting a new one.
// Without this, rapid triggers (auto_advance period < transition_time, or a manual
// 'next' on top of an auto-cycle) keep resetting mTransitionProgress to 0 on the same
// pending index → the transition never completes, mCurrentIndex never moves, and the
// texture appears frozen. Committing first makes successive triggers chain forward.
void TextureCycleNode::commitPendingTransition() {
    if (mNextIndex != mCurrentIndex
     && mNextIndex >= 0 && mNextIndex < static_cast<int>(mTextures.size())) {
        mCurrentIndex = mNextIndex;
    }
    mTransitionProgress = 0.0f;
}

void TextureCycleNode::triggerNext() {
    if (mTextures.empty()) return;
    commitPendingTransition();
    if (mMode == Mode::Random) {
        mNextIndex = pickRandomNext();
    } else {
        mNextIndex = (mCurrentIndex + 1) % static_cast<int>(mTextures.size());
    }
    mTransitionProgress = 0.0f;
}

void TextureCycleNode::triggerPrev() {
    if (mTextures.empty()) return;
    commitPendingTransition();
    int n = static_cast<int>(mTextures.size());
    if (mMode == Mode::Random) {
        mNextIndex = pickRandomNext();
    } else {
        mNextIndex = (mCurrentIndex - 1 + n) % n;
    }
    mTransitionProgress = 0.0f;
}

void TextureCycleNode::triggerGoto(int idx) {
    if (mTextures.empty()) return;
    commitPendingTransition();
    int n = static_cast<int>(mTextures.size());
    if (idx < 0) idx = 0;
    if (idx >= n) idx = n - 1;
    mNextIndex = idx;
    mTransitionProgress = 0.0f;
}

void TextureCycleNode::update() {
    if (!mEnabled) return;

    // Parse textures CSV from ParamSpec if it changed.
    auto* texParam = mSpec.getParam("textures");
    if (texParam) {
        // Cheap change detection: compare with the cached oss form via getTextures.
        std::ostringstream cur;
        for (size_t i = 0; i < mTextures.size(); ++i) {
            if (i) cur << ';';
            cur << mTextures[i];
        }
        if (cur.str() != texParam->stringVal) {
            std::vector<std::string> parsed;
            std::istringstream iss(texParam->stringVal);
            std::string tok;
            while (std::getline(iss, tok, ';')) {
                if (!tok.empty()) parsed.push_back(tok);
            }
            setTextures(parsed);
        }
    }

    // Sync ParamSpec -> internal state
    auto* modeParam = mSpec.getParam("mode");
    if (modeParam) {
        if      (modeParam->stringVal == "random")          mMode = Mode::Random;
        else if (modeParam->stringVal == "bpm_synced")      mMode = Mode::BpmSynced;
        else if (modeParam->stringVal == "audio_triggered") mMode = Mode::AudioTriggered;
        else                                                mMode = Mode::Sequential;
    }
    auto* tParam = mSpec.getParam("transition_time");
    if (tParam) mTransitionTime = (tParam->floatVal > 0.0f) ? tParam->floatVal : 1.0f;
    auto* bpmParam = mSpec.getParam("auto_advance_bpm");
    if (bpmParam) mAutoAdvanceBpm = bpmParam->floatVal;
    auto* seedParam = mSpec.getParam("random_seed");
    if (seedParam && seedParam->intVal != mRandomSeed) {
        mRandomSeed = seedParam->intVal;
        mRngInitialized = false; // re-seed at next pickRandomNext()
    }

    auto& in = getInputs();

    // Detect external 'index' setter: only react when the input value CHANGES.
    // (Without delta detection, a port stuck at 0 would constantly reset the cycle.)
    auto itIdx = in.find("index");
    if (itIdx != in.end()) {
        int requested = static_cast<int>(itIdx->second->getValue() + 0.5f);
        if (requested != mPrevIndexInput) {
            if (!mTextures.empty()
             && requested >= 0 && requested < static_cast<int>(mTextures.size())) {
                mCurrentIndex = requested;
                mNextIndex = requested;
                mTransitionProgress = 0.0f;
            }
            mPrevIndexInput = requested;
        }
    }

    // Edge-detect triggers
    auto itNext = in.find("next");
    if (itNext != in.end()) {
        bool s = (itNext->second->getValue() > 0.5f);
        if (s && !mPrevNextState) triggerNext();
        mPrevNextState = s;
    }
    auto itPrev = in.find("prev");
    if (itPrev != in.end()) {
        bool s = (itPrev->second->getValue() > 0.5f);
        if (s && !mPrevPrevState) triggerPrev();
        mPrevPrevState = s;
    }
    auto itGoto = in.find("goto_index");
    if (itGoto != in.end()) {
        // goto_index — port défaut -1.0 (sentinel "no request"). NE PAS appliquer
        // `+0.5f` AVANT le test de signe : static_cast<int>(-1.0+0.5)=0 (troncature
        // vers 0) → triggerGoto(0) spurieux à la frame 1. (réf. TextureSetNode)
        float gv = itGoto->second->getValue();
        if (gv >= 0.0f) {
            int g = static_cast<int>(gv + 0.5f);
            if (g != mPrevGotoIndex) {
                triggerGoto(g);
                mPrevGotoIndex = g;
            }
        }
    }
    if (mMode == Mode::AudioTriggered) {
        auto itBeat = in.find("beat");
        if (itBeat != in.end()) {
            bool s = (itBeat->second->getValue() > 0.5f);
            if (s && !mPrevBeatState) triggerNext();
            mPrevBeatState = s;
        }
    }

    // BPM-synced auto advance — uses dt port if available (fallback 1/60).
    if (mMode == Mode::BpmSynced && mAutoAdvanceBpm > 0.0f) {
        float dt = 0.0f;
        auto itDt = in.find("dt");
        if (itDt != in.end()) dt = itDt->second->getValue();
        if (dt <= 0.0f) dt = (mPrevDt > 0.0f) ? mPrevDt : (1.0f / 60.0f);
        else mPrevDt = dt;

        auto* root = RootTimeNode::instance();
        float bpm = root ? root->getBPM() : 120.0f;
        if (bpm <= 0.0f) bpm = 120.0f;
        float period = 60.0f / (bpm * mAutoAdvanceBpm);
        if (period <= 0.0f) period = 1.0f;
        mBpmAccumulator += dt;
        while (mBpmAccumulator >= period) {
            triggerNext();
            mBpmAccumulator -= period;
        }
    }

    // Advance transition
    if (mNextIndex != mCurrentIndex) {
        float dt = 0.0f;
        auto itDt = in.find("dt");
        if (itDt != in.end()) dt = itDt->second->getValue();
        if (dt <= 0.0f) dt = (mPrevDt > 0.0f) ? mPrevDt : (1.0f / 60.0f);
        mTransitionProgress += dt / mTransitionTime;
        if (mTransitionProgress >= 1.0f) {
            mCurrentIndex = mNextIndex;
            mTransitionProgress = 0.0f;
        }
    }

    // Update outputs + ParamSpec mirrors
    auto& out = getOutputs();
    auto itCurOut = out.find("current_index");
    if (itCurOut != out.end()) itCurOut->second->setValue(static_cast<float>(mCurrentIndex));
    auto itNxtOut = out.find("next_index");
    if (itNxtOut != out.end()) itNxtOut->second->setValue(static_cast<float>(mNextIndex));
    auto itPrgOut = out.find("transition_progress");
    if (itPrgOut != out.end()) itPrgOut->second->setValue(mTransitionProgress);
    auto itRdyOut = out.find("texture_ready");
    if (itRdyOut != out.end()) {
        // Active si une transition est en cours (progress > 0 et < 1).
        bool active = (mTransitionProgress > 0.001f && mTransitionProgress < 0.999f);
        itRdyOut->second->setValue(active ? 1.0f : 0.0f);
    }

    auto* curMir = mSpec.getParam("current_texture");
    if (curMir) curMir->stringVal = getCurrentTexture();
    auto* nxtMir = mSpec.getParam("next_texture");
    if (nxtMir) nxtMir->stringVal = getNextTexture();

    fireUpdate();
}

} // namespace bbfx
