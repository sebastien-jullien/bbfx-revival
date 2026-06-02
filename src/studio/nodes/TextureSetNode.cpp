#include "TextureSetNode.h"
#include "../../core/PrimitiveNodes.h"
#include "../../core/DebugLog.h"
#include <iostream>
#include <sstream>

namespace bbfx {

TextureSetNode::TextureSetNode(const std::string& name)
    : AnimationNode(name)
{
    // ── ParamSpec ──
    ParamDef presetsDef;
    presetsDef.name = "presets"; presetsDef.label = "Presets (CSV)";
    presetsDef.type = ParamType::STRING;
    presetsDef.stringVal = "";
    presetsDef.tooltip = "Couples 'gray|color|factor' séparés par ';'. "
                         "factor = multiplicateur de vitesse pour layer B (négatif = sens opposé).";
    mSpec.addParam(presetsDef);

    ParamDef tDef;
    tDef.name = "transition_time"; tDef.label = "Transition Time (s)";
    tDef.type = ParamType::FLOAT;
    tDef.floatVal = 1.0f; tDef.minVal = 0.0f; tDef.maxVal = 10.0f;
    tDef.tooltip = "Durée du fondu entre deux couples du set.";
    mSpec.addParam(tDef);

    ParamDef curIdxDef;
    curIdxDef.name = "current_index_param"; curIdxDef.label = "Current Index";
    curIdxDef.type = ParamType::INT;
    curIdxDef.intVal = 0; curIdxDef.minVal = 0; curIdxDef.maxVal = 999;
    curIdxDef.readOnly = true;
    curIdxDef.tooltip = "Read-only: index du couple courant.";
    mSpec.addParam(curIdxDef);

    ParamDef curADef;
    curADef.name = "current_texture_a"; curADef.label = "Current A (gray)";
    curADef.type = ParamType::STRING; curADef.stringVal = "";
    curADef.readOnly = true;
    curADef.tooltip = "Read-only: nom de la texture courante côté layer A (gray du couple).";
    mSpec.addParam(curADef);

    ParamDef curBDef;
    curBDef.name = "current_texture_b"; curBDef.label = "Current B (color)";
    curBDef.type = ParamType::STRING; curBDef.stringVal = "";
    curBDef.readOnly = true;
    curBDef.tooltip = "Read-only: nom de la texture courante côté layer B (color du couple).";
    mSpec.addParam(curBDef);

    ParamDef curFDef;
    curFDef.name = "current_factor"; curFDef.label = "Current Factor";
    curFDef.type = ParamType::FLOAT;
    curFDef.floatVal = 1.0f;
    curFDef.readOnly = true;
    curFDef.tooltip = "Read-only: facteur courant (multiplicateur scroll layer B).";
    mSpec.addParam(curFDef);

    setParamSpec(&mSpec);

    // ── Ports IN ──
    addInput(new AnimationPort("dt", 1.0f / 60.0f));
    addInput(new AnimationPort("next", 0.0f));
    addInput(new AnimationPort("prev", 0.0f));
    addInput(new AnimationPort("goto_index", -1.0f));

    // ── Ports OUT ──
    addOutput(new AnimationPort("texture_a_ready",   0.0f));
    addOutput(new AnimationPort("texture_b_ready",   0.0f));
    addOutput(new AnimationPort("factor",            1.0f));
    addOutput(new AnimationPort("current_index",     0.0f));
    addOutput(new AnimationPort("transition_progress", 0.0f));

    // Port tooltips
    setPortTooltip("next",        "Trigger (front montant) : passe au couple suivant {gray, color, factor}.");
    setPortTooltip("prev",        "Trigger (front montant) : couple précédent.");
    setPortTooltip("goto_index",  "Index direct du couple cible. -1 = no-op.");
    setPortTooltip("dt",          "Frame delta (s). Linké depuis time.dt.");
    setPortTooltip("texture_a_ready",   "Pulse 1.0 pendant transition active. À linker sur TextureBlendNode.tex_a_source (Pattern 3, lit current_texture_a).");
    setPortTooltip("texture_b_ready",   "Pulse 1.0 pendant transition active. À linker sur TextureBlendNode.tex_b_source.");
    setPortTooltip("factor",      "Multiplicateur courant de vitesse pour layer B. À linker sur TextureBlendNode.factor.");
    setPortTooltip("current_index",     "Index entier courant (output).");
    setPortTooltip("transition_progress", "Fade progress 0..1 (1 = couple suivant atteint).");
}

void TextureSetNode::setPresets(const std::vector<Preset>& list) {
    mPresets = list;
    if (mCurrentIndex >= static_cast<int>(mPresets.size())) mCurrentIndex = 0;
    if (mNextIndex    >= static_cast<int>(mPresets.size())) mNextIndex    = mCurrentIndex;

    // Mirror back into ParamSpec
    std::ostringstream oss;
    for (size_t i = 0; i < mPresets.size(); ++i) {
        if (i) oss << ';';
        oss << mPresets[i].gray << '|' << mPresets[i].color << '|' << mPresets[i].factor;
    }
    auto* p = mSpec.getParam("presets");
    if (p) p->stringVal = oss.str();
    mPrevPresetsStr = oss.str();
}

float TextureSetNode::getCurrentFactor() const {
    if (mPresets.empty()) return 1.0f;
    int n = static_cast<int>(mPresets.size());
    int i = (mCurrentIndex >= 0 && mCurrentIndex < n) ? mCurrentIndex : 0;
    return mPresets[i].factor;
}

const std::string& TextureSetNode::getCurrentTextureA() const {
    static const std::string empty;
    if (mPresets.empty()) return empty;
    int n = static_cast<int>(mPresets.size());
    int i = (mCurrentIndex >= 0 && mCurrentIndex < n) ? mCurrentIndex : 0;
    return mPresets[i].gray;
}

const std::string& TextureSetNode::getCurrentTextureB() const {
    static const std::string empty;
    if (mPresets.empty()) return empty;
    int n = static_cast<int>(mPresets.size());
    int i = (mCurrentIndex >= 0 && mCurrentIndex < n) ? mCurrentIndex : 0;
    return mPresets[i].color;
}

// Commit la transition pendante avant d'en lancer une nouvelle — sinon des
// triggers rapprochés (auto-advance + bouton manuel) bloqueraient le cycle
// sur le même couple (regression GPC-009 chez TextureCycleNode).
void TextureSetNode::commitPendingTransition() {
    if (mNextIndex != mCurrentIndex
     && mNextIndex >= 0 && mNextIndex < static_cast<int>(mPresets.size())) {
        mCurrentIndex = mNextIndex;
    }
    mTransitionProgress = 0.0f;
}

void TextureSetNode::triggerNext() {
    if (mPresets.empty()) return;
    commitPendingTransition();
    int n = static_cast<int>(mPresets.size());
    mNextIndex = (mCurrentIndex + 1) % n;
    mTransitionProgress = 0.0f;
}

void TextureSetNode::triggerPrev() {
    if (mPresets.empty()) return;
    commitPendingTransition();
    int n = static_cast<int>(mPresets.size());
    mNextIndex = (mCurrentIndex - 1 + n) % n;
    mTransitionProgress = 0.0f;
}

void TextureSetNode::triggerGoto(int idx) {
    if (mPresets.empty()) return;
    commitPendingTransition();
    int n = static_cast<int>(mPresets.size());
    if (idx < 0) idx = 0;
    if (idx >= n) idx = n - 1;
    mNextIndex = idx;
    mTransitionProgress = 0.0f;
}

void TextureSetNode::parsePresetsFromParam() {
    auto* p = mSpec.getParam("presets");
    if (!p) return;
    if (p->stringVal == mPrevPresetsStr) return;   // unchanged
    mPrevPresetsStr = p->stringVal;

    std::vector<Preset> parsed;
    std::istringstream iss(p->stringVal);
    std::string tok;
    while (std::getline(iss, tok, ';')) {
        if (tok.empty()) continue;
        // tok = "gray|color|factor"
        std::istringstream pi(tok);
        std::string g, c, fs;
        if (!std::getline(pi, g, '|')) continue;
        if (!std::getline(pi, c, '|')) continue;
        if (!std::getline(pi, fs, '|')) fs = "1.0";
        float fac = 1.0f;
        try { fac = std::stof(fs); } catch (...) { fac = 1.0f; }
        parsed.push_back({g, c, fac});
    }
    // setPresets re-écrit mirror; pour éviter une recursion infinie sur le
    // diff, on met à jour mPrevPresetsStr d'abord (déjà fait) puis on assigne.
    mPresets = parsed;
    if (mCurrentIndex >= static_cast<int>(mPresets.size())) mCurrentIndex = 0;
    if (mNextIndex    >= static_cast<int>(mPresets.size())) mNextIndex    = mCurrentIndex;
}

void TextureSetNode::update() {
    if (!mEnabled) return;

    parsePresetsFromParam();

    // Sync transition_time depuis ParamSpec
    auto* tParam = mSpec.getParam("transition_time");
    if (tParam) mTransitionTime = (tParam->floatVal > 0.0f) ? tParam->floatVal : 1.0f;

    auto& in = getInputs();

    // Edge-detect next
    auto itNext = in.find("next");
    if (itNext != in.end()) {
        bool s = (itNext->second->getValue() > 0.5f);
        // diag gaté (BBFX_DEBUG_LOG). L'edge est détecté via le membre per-instance
        // mPrevNextState (l'ancien `static bool s_dbg_prev` était partagé entre TOUTES
        // les instances → log faux en multi-node).
        if (s != mPrevNextState) {
            BBFX_DLOG("[TS " << getName() << "] next port: " << (mPrevNextState ? "1" : "0")
                      << " -> " << (s ? "1" : "0")
                      << " (rawValue=" << itNext->second->getValue() << ")");
        }
        if (s && !mPrevNextState) {
            BBFX_DLOG("[TS " << getName() << "] triggerNext() FIRES (was idx=" << mCurrentIndex << ")");
            triggerNext();
        }
        mPrevNextState = s;
    }
    // Edge-detect prev
    auto itPrev = in.find("prev");
    if (itPrev != in.end()) {
        bool s = (itPrev->second->getValue() > 0.5f);
        if (s && !mPrevPrevState) triggerPrev();
        mPrevPrevState = s;
    }
    // goto_index — port défaut -1.0 (sentinel "no request"). Important : NE PAS
    // appliquer `+0.5f` AVANT le test de signe — `static_cast<int>(-1.0 + 0.5)`
    // = `static_cast<int>(-0.5)` = 0 (troncature vers 0), ce qui ferait un
    // triggerGoto(0) spurieux à chaque update et écraserait mNextIndex.
    auto itGoto = in.find("goto_index");
    if (itGoto != in.end()) {
        float gv = itGoto->second->getValue();
        if (gv >= 0.0f) {
            int g = static_cast<int>(gv + 0.5f);
            if (g != mPrevGotoIndex) {
                triggerGoto(g);
                mPrevGotoIndex = g;
            }
        }
    }

    // Avance transition
    if (mNextIndex != mCurrentIndex) {
        float dt = 0.0f;
        auto itDt = in.find("dt");
        if (itDt != in.end()) dt = itDt->second->getValue();
        if (dt <= 0.0f) dt = (mPrevDt > 0.0f) ? mPrevDt : (1.0f / 60.0f);
        else mPrevDt = dt;
        mTransitionProgress += dt / mTransitionTime;
        if (mTransitionProgress >= 1.0f) {
            mCurrentIndex = mNextIndex;
            mTransitionProgress = 0.0f;
        }
    }

    // Outputs
    auto& out = getOutputs();
    bool active = (mTransitionProgress > 0.001f && mTransitionProgress < 0.999f);
    if (auto it = out.find("texture_a_ready"); it != out.end())
        it->second->setValue(active ? 1.0f : 0.0f);
    if (auto it = out.find("texture_b_ready"); it != out.end())
        it->second->setValue(active ? 1.0f : 0.0f);
    if (auto it = out.find("factor"); it != out.end())
        it->second->setValue(getCurrentFactor());
    if (auto it = out.find("current_index"); it != out.end())
        it->second->setValue(static_cast<float>(mCurrentIndex));
    if (auto it = out.find("transition_progress"); it != out.end())
        it->second->setValue(mTransitionProgress);

    // ParamSpec mirrors
    if (auto* p = mSpec.getParam("current_texture_a")) p->stringVal = getCurrentTextureA();
    if (auto* p = mSpec.getParam("current_texture_b")) p->stringVal = getCurrentTextureB();
    if (auto* p = mSpec.getParam("current_factor"))    p->floatVal  = getCurrentFactor();
    if (auto* p = mSpec.getParam("current_index_param")) p->intVal  = mCurrentIndex;

    fireUpdate();
}

} // namespace bbfx
