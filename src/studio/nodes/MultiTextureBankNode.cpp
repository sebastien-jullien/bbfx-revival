#include "MultiTextureBankNode.h"
#include "../../core/PrimitiveNodes.h"
#include <algorithm>
#include <chrono>
#include <sstream>

namespace bbfx {

MultiTextureBankNode::MultiTextureBankNode(const std::string& name)
    : AnimationNode(name)
{
    ParamDef pDef;
    pDef.name = "presets"; pDef.label = "Presets ('|'-cols, ';'-rows)";
    pDef.type = ParamType::STRING; pDef.stringVal = "";
    mSpec.addParam(pDef);

    ParamDef slotsDef;
    slotsDef.name = "slot_count"; slotsDef.label = "Slot Count";
    slotsDef.type = ParamType::INT;
    slotsDef.intVal = 2; slotsDef.minVal = 1; slotsDef.maxVal = 4;
    mSpec.addParam(slotsDef);

    ParamDef modeDef;
    modeDef.name = "mode"; modeDef.label = "Mode"; modeDef.type = ParamType::ENUM;
    modeDef.stringVal = "sequential";
    modeDef.choices = {"sequential", "random", "bpm_synced"};
    mSpec.addParam(modeDef);

    // Read-only mirrors of the slot textures (so other nodes / Inspector can read them).
    for (int i = 0; i < 4; ++i) {
        ParamDef sDef;
        sDef.name = "slot_" + std::to_string(i) + "_texture";
        sDef.label = "Slot " + std::to_string(i);
        sDef.type = ParamType::STRING; sDef.stringVal = "";
        sDef.readOnly = true;
        sDef.tooltip = "Read-only: live texture name in slot " + std::to_string(i)
                     + ". Routable to MaterialBridgeNode auto-wrap.";
        mSpec.addParam(sDef);
    }

    setParamSpec(&mSpec);

    addInput(new AnimationPort("dt",            1.0f / 60.0f));
    addInput(new AnimationPort("preset_index",  0.0f));
    addInput(new AnimationPort("next_preset",   0.0f));
    addInput(new AnimationPort("prev_preset",   0.0f));
    addInput(new AnimationPort("goto_index",   -1.0f));

    addOutput(new AnimationPort("current_preset_index", 0.0f));

    // v3.5.2 Sprint S7 Lot Z — port tooltips
    setPortTooltip("preset_index",         "Direct integer index of the active preset (overrides next/prev triggers).");
    setPortTooltip("next_preset",          "Trigger to advance to the next preset row (with wrap).");
    setPortTooltip("prev_preset",          "Trigger to go back to the previous preset row.");
    setPortTooltip("goto_index",           "Floored integer target. -1 = no request.");
    setPortTooltip("current_preset_index", "Active preset index (output, mirrors preset_index after edge resolution).");
}

const std::string& MultiTextureBankNode::getSlotTexture(int slot) const {
    static const std::string empty;
    if (mPresets.empty()) return empty;
    if (mPresetIndex < 0 || mPresetIndex >= (int)mPresets.size()) return empty;
    if (slot < 0 || slot >= (int)mPresets[mPresetIndex].size()) return empty;
    return mPresets[mPresetIndex][slot];
}

void MultiTextureBankNode::seedRngIfNeeded() {
    if (mRngInit) return;
    auto t = std::chrono::high_resolution_clock::now().time_since_epoch().count();
    mRng.seed(static_cast<uint32_t>(t) ^ static_cast<uint32_t>(reinterpret_cast<uintptr_t>(this)));
    mRngInit = true;
}

int MultiTextureBankNode::pickRandomNext() {
    seedRngIfNeeded();
    int n = (int)mPresets.size();
    if (n <= 1) return 0;
    std::uniform_int_distribution<int> dist(0, n - 2);
    int pick = dist(mRng);
    if (pick >= mPresetIndex) pick++;
    return pick;
}

void MultiTextureBankNode::parsePresetsIfChanged() {
    auto* pParam = mSpec.getParam("presets");
    auto* sParam = mSpec.getParam("slot_count");
    if (!pParam) return;
    if (pParam->stringVal == mPrevPresetsCsv && sParam && sParam->intVal == mPrevSlotCount) return;

    mPrevPresetsCsv = pParam->stringVal;
    mSlotCount = sParam ? std::clamp(sParam->intVal, 1, 4) : 2;
    mPrevSlotCount = mSlotCount;

    mPresets.clear();
    std::istringstream rows(mPrevPresetsCsv);
    std::string row;
    while (std::getline(rows, row, ';')) {
        if (row.empty()) continue;
        std::vector<std::string> cols;
        std::istringstream c(row);
        std::string col;
        while (std::getline(c, col, '|')) {
            if (!col.empty()) cols.push_back(col);
        }
        if (!cols.empty()) {
            // Pad to slot_count with empty strings (or last)
            while ((int)cols.size() < mSlotCount) cols.push_back("");
            mPresets.push_back(std::move(cols));
        }
    }
    if (mPresetIndex >= (int)mPresets.size()) mPresetIndex = 0;
}

void MultiTextureBankNode::writeMirrors() {
    for (int i = 0; i < 4; ++i) {
        auto* mir = mSpec.getParam("slot_" + std::to_string(i) + "_texture");
        if (!mir) continue;
        if (i < (int)mSlotCount) {
            mir->stringVal = getSlotTexture(i);
        } else {
            mir->stringVal = "";
        }
    }
}

void MultiTextureBankNode::update() {
    if (!mEnabled) return;

    parsePresetsIfChanged();

    auto* modeParam = mSpec.getParam("mode");
    if (modeParam) {
        if      (modeParam->stringVal == "random")     mMode = Mode::Random;
        else if (modeParam->stringVal == "bpm_synced") mMode = Mode::BpmSynced;
        else                                           mMode = Mode::Sequential;
    }

    auto& in = getInputs();
    auto getf = [&in](const char* n, float d) {
        auto it = in.find(n);
        return (it != in.end()) ? it->second->getValue() : d;
    };

    // External index port (delta-detected)
    int reqIdx = static_cast<int>(getf("preset_index", 0.0f) + 0.5f);
    if (reqIdx != mPrevPresetIndexInput) {
        if (!mPresets.empty() && reqIdx >= 0 && reqIdx < (int)mPresets.size()) {
            mPresetIndex = reqIdx;
        }
        mPrevPresetIndexInput = reqIdx;
    }

    auto edge = [](float v, bool& prev) {
        bool s = (v > 0.5f);
        bool e = (s && !prev);
        prev = s;
        return e;
    };
    if (edge(getf("next_preset", 0.0f), mPrevNextState) && !mPresets.empty()) {
        if (mMode == Mode::Random) mPresetIndex = pickRandomNext();
        else                       mPresetIndex = (mPresetIndex + 1) % (int)mPresets.size();
    }
    if (edge(getf("prev_preset", 0.0f), mPrevPrevState) && !mPresets.empty()) {
        int n = (int)mPresets.size();
        if (mMode == Mode::Random) mPresetIndex = pickRandomNext();
        else                       mPresetIndex = (mPresetIndex - 1 + n) % n;
    }
    // goto_index — port défaut -1.0 (sentinel "no request"). NE PAS appliquer
    // `+0.5f` AVANT le test de signe : static_cast<int>(-1.0+0.5)=static_cast<int>(-0.5)=0
    // (troncature vers 0) → triggerGoto(0) spurieux à la frame 1. (réf. TextureSetNode)
    float gv = getf("goto_index", -1.0f);
    if (gv >= 0.0f) {
        int g = static_cast<int>(gv + 0.5f);
        if (g != mPrevGotoIndex) {
            if (!mPresets.empty()) mPresetIndex = std::clamp(g, 0, (int)mPresets.size() - 1);
            mPrevGotoIndex = g;
        }
    }

    if (mMode == Mode::BpmSynced && !mPresets.empty()) {
        float dt = getf("dt", 0.0f);
        if (dt <= 0.0f) dt = mPrevDt;
        else mPrevDt = dt;
        auto* root = RootTimeNode::instance();
        float bpm = root ? root->getBPM() : 120.0f;
        if (bpm <= 0.0f) bpm = 120.0f;
        float period = 60.0f / bpm; // 1 advance per quarter
        mBpmAccumulator += dt;
        while (mBpmAccumulator >= period) {
            mPresetIndex = (mPresetIndex + 1) % (int)mPresets.size();
            mBpmAccumulator -= period;
        }
    }

    auto& out = getOutputs();
    if (out.count("current_preset_index"))
        out.at("current_preset_index")->setValue((float)mPresetIndex);

    writeMirrors();

    fireUpdate();
}

} // namespace bbfx

