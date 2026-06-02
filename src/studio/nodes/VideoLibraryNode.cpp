#include "VideoLibraryNode.h"
#include "../../core/PrimitiveNodes.h"
#include <iostream>
#include <sstream>
#include <algorithm>

namespace bbfx {

VideoLibraryNode::VideoLibraryNode(const std::string& name)
    : AnimationNode(name)
{
    ParamDef clipsDef;
    clipsDef.name = "clips"; clipsDef.label = "Clips (CSV)";
    clipsDef.type = ParamType::STRING; clipsDef.stringVal = "";
    mSpec.addParam(clipsDef);

    ParamDef pmDef;
    pmDef.name = "play_mode"; pmDef.label = "Play Mode";
    pmDef.type = ParamType::ENUM;
    pmDef.stringVal = "loop";
    pmDef.choices = {"loop", "once", "ping_pong"};
    mSpec.addParam(pmDef);

    ParamDef bpmDef;
    bpmDef.name = "bpm_sync"; bpmDef.label = "BPM Sync";
    bpmDef.type = ParamType::BOOL; bpmDef.boolVal = false;
    mSpec.addParam(bpmDef);

    // M4 — param `volume` retiré : TheoraClip décode de la VIDÉO seule (Theora),
    // il n'existe aucun pipeline audio (Ogg/Vorbis) ni mixer dans le moteur. Un
    // contrôle de volume ne pouvait donc rien piloter → on ne propose pas un
    // contrôle non fonctionnel. (Réintroduire avec un vrai décodage audio = feature
    // majeure à planifier en roadmap si souhaité.)

    ParamDef nameMir;
    nameMir.name = "current_name"; nameMir.label = "Current";
    nameMir.type = ParamType::STRING; nameMir.stringVal = "";
    mSpec.addParam(nameMir);

    ParamDef matMir;
    matMir.name = "material_out"; matMir.label = "Material (out)";
    matMir.type = ParamType::STRING; matMir.stringVal = "";
    matMir.readOnly = true;
    matMir.tooltip = "Read-only: live material name from the currently-selected library clip.";
    mSpec.addParam(matMir);

    setParamSpec(&mSpec);

    addInput(new AnimationPort("dt",         1.0f / 60.0f));
    addInput(new AnimationPort("index",      0.0f));
    addInput(new AnimationPort("next",       0.0f));
    addInput(new AnimationPort("prev",       0.0f));
    addInput(new AnimationPort("goto_index", -1.0f));
    addInput(new AnimationPort("play",       0.0f));
    addInput(new AnimationPort("pause",      0.0f));
    addInput(new AnimationPort("stop",       0.0f));
    addInput(new AnimationPort("seek",       -1.0f));

    addOutput(new AnimationPort("material_ready", 0.0f));
    addOutput(new AnimationPort("time",           0.0f));
    addOutput(new AnimationPort("progress",       0.0f));
    addOutput(new AnimationPort("current_index",  0.0f));
    addOutput(new AnimationPort("duration",       0.0f));
    addOutput(new AnimationPort("is_playing",     0.0f));

    // v3.5.2 Sprint S7 Lot Z — port tooltips
    setPortTooltip("index",          "Direct integer index into the clip array (overrides next/prev).");
    setPortTooltip("next",           "Trigger to advance to the next clip in the bank.");
    setPortTooltip("prev",           "Trigger to go to the previous clip.");
    setPortTooltip("goto_index",     "Floored integer target. -1 = no request.");
    setPortTooltip("play",           "Trigger play/resume on the active clip.");
    setPortTooltip("pause",          "Trigger pause on the active clip.");
    setPortTooltip("stop",           "Trigger stop + rewind on the active clip.");
    setPortTooltip("seek",           "Seek time in seconds. -1 = no request.");
    setPortTooltip("material_ready", "Niveau 1.0 tant que le matériau du clip actif est lié.");
    setPortTooltip("time",           "Active clip playhead position in seconds.");
    setPortTooltip("progress",       "Active clip progress 0..1 (time / duration).");
    setPortTooltip("current_index",  "Active clip index (output).");
    setPortTooltip("duration",       "Active clip duration in seconds.");
    setPortTooltip("is_playing",     "1.0 if active clip is currently playing, 0.0 otherwise.");
}

std::string VideoLibraryNode::basename(const std::string& path) {
    size_t lastSlash = path.find_last_of("/\\");
    std::string name = (lastSlash == std::string::npos) ? path : path.substr(lastSlash + 1);
    size_t dot = name.find_last_of('.');
    if (dot != std::string::npos) name = name.substr(0, dot);
    return name;
}

std::string VideoLibraryNode::parentDir(const std::string& path) {
    size_t lastSlash = path.find_last_of("/\\");
    return (lastSlash == std::string::npos) ? std::string() : path.substr(0, lastSlash);
}

void VideoLibraryNode::parseClipsIfChanged() {
    auto* clipsParam = mSpec.getParam("clips");
    if (!clipsParam) return;
    if (clipsParam->stringVal == mPrevClipsCsv) return;

    // Drop existing clips (let unique_ptr destructors free TheoraReader/Blitter).
    mClips.clear();
    mPrevClipsCsv = clipsParam->stringVal;

    std::istringstream iss(mPrevClipsCsv);
    std::string token;
    while (std::getline(iss, token, ';')) {
        if (token.empty()) continue;
        ClipEntry e;
        e.fullPath = token;
        e.filename = basename(token);
        try {
            // Tag with library node name + index so 2 libraries referencing
            // the same file produce distinct OGRE texture/material names.
            e.clip = std::make_unique<TheoraClip>(
                token, getName() + "_" + std::to_string(mClips.size()));
            e.loaded = true;
        } catch (...) {
            std::cerr << "[VideoLibraryNode] Failed to load clip: " << token
                      << " — entry kept as placeholder" << std::endl;
            e.loaded = false;
        }
        mClips.push_back(std::move(e));
    }
    if (mCurrentIndex >= (int)mClips.size()) mCurrentIndex = 0;
}

void VideoLibraryNode::switchTo(int newIdx) {
    if (newIdx == mCurrentIndex) return;
    if (newIdx < 0 || newIdx >= (int)mClips.size()) return;

    // Pause the previous clip (preserves seek position by default in TheoraClip).
    if (mCurrentIndex >= 0 && mCurrentIndex < (int)mClips.size()
     && mClips[mCurrentIndex].clip && mClips[mCurrentIndex].loaded) {
        mClips[mCurrentIndex].clip->pause();
    }

    mCurrentIndex = newIdx;

    if (mClips[mCurrentIndex].clip && mClips[mCurrentIndex].loaded) {
        mClips[mCurrentIndex].clip->setLoop(mPlayMode == PlayMode::Loop
                                          || mPlayMode == PlayMode::PingPong);
        mClips[mCurrentIndex].clip->play();
    }
}

const std::string& VideoLibraryNode::getCurrentName() const {
    static const std::string empty;
    if (mClips.empty() || mCurrentIndex < 0 || mCurrentIndex >= (int)mClips.size())
        return empty;
    return mClips[mCurrentIndex].filename;
}

const std::string& VideoLibraryNode::getCurrentMaterialName() const {
    static const std::string empty;
    if (mClips.empty() || mCurrentIndex < 0 || mCurrentIndex >= (int)mClips.size()
     || !mClips[mCurrentIndex].clip || !mClips[mCurrentIndex].loaded)
        return empty;
    return mClips[mCurrentIndex].clip->getMaterialName();
}

bool VideoLibraryNode::isCurrentPlaying() const {
    if (mClips.empty() || mCurrentIndex < 0 || mCurrentIndex >= (int)mClips.size()
     || !mClips[mCurrentIndex].clip)
        return false;
    return mClips[mCurrentIndex].clip->isPlaying();
}

void VideoLibraryNode::update() {
    if (!mEnabled) return;

    parseClipsIfChanged();

    // Sync ParamSpec
    auto* pmParam = mSpec.getParam("play_mode");
    if (pmParam) {
        if      (pmParam->stringVal == "once")      mPlayMode = PlayMode::Once;
        else if (pmParam->stringVal == "ping_pong") mPlayMode = PlayMode::PingPong;
        else                                         mPlayMode = PlayMode::Loop;
    }
    auto* bpmParam = mSpec.getParam("bpm_sync");
    if (bpmParam) mBpmSync = bpmParam->boolVal;

    auto& in = getInputs();
    auto getf = [&in](const char* name, float def) {
        auto it = in.find(name);
        return (it != in.end()) ? it->second->getValue() : def;
    };

    // dt for BPM accumulator and frame update
    float dt = getf("dt", 0.0f);
    if (dt <= 0.0f) dt = mPrevDt;
    else mPrevDt = dt;

    // External index port (delta-detected to avoid stuck-at-0 reset)
    int requestedIdx = static_cast<int>(getf("index", 0.0f) + 0.5f);
    if (requestedIdx != mPrevIndexInput) {
        if (requestedIdx >= 0 && requestedIdx < (int)mClips.size()) {
            switchTo(requestedIdx);
        }
        mPrevIndexInput = requestedIdx;
    }

    // Edge-detected triggers
    auto edge = [](float val, bool& prev) {
        bool s = (val > 0.5f);
        bool e = (s && !prev);
        prev = s;
        return e;
    };
    if (edge(getf("next", 0.0f), mPrevNextState)) {
        if (!mClips.empty()) switchTo((mCurrentIndex + 1) % (int)mClips.size());
    }
    if (edge(getf("prev", 0.0f), mPrevPrevState)) {
        int n = (int)mClips.size();
        if (n > 0) switchTo((mCurrentIndex - 1 + n) % n);
    }
    int gIdx = static_cast<int>(getf("goto_index", -1.0f) + 0.5f);
    if (gIdx >= 0 && gIdx != mPrevGotoIndex) {
        switchTo(gIdx);
        mPrevGotoIndex = gIdx;
    }
    if (edge(getf("play", 0.0f), mPrevPlayState)) {
        if (!mClips.empty() && mClips[mCurrentIndex].clip) mClips[mCurrentIndex].clip->play();
    }
    if (edge(getf("pause", 0.0f), mPrevPauseState)) {
        if (!mClips.empty() && mClips[mCurrentIndex].clip) mClips[mCurrentIndex].clip->pause();
    }
    if (edge(getf("stop", 0.0f), mPrevStopState)) {
        if (!mClips.empty() && mClips[mCurrentIndex].clip) mClips[mCurrentIndex].clip->stop();
    }
    float seek = getf("seek", -1.0f);
    if (seek >= 0.0f && std::abs(seek - mPrevSeek) > 1e-4f) {
        if (!mClips.empty() && mClips[mCurrentIndex].clip) {
            mClips[mCurrentIndex].clip->setTime(seek); // direct time set; absolute seconds.
        }
        mPrevSeek = seek;
    }

    // BPM-synced auto next: trigger next every half-bar (period = 60 * 2 / BPM).
    if (mBpmSync && !mClips.empty()) {
        auto* root = RootTimeNode::instance();
        float bpm = root ? root->getBPM() : 120.0f;
        if (bpm <= 0.0f) bpm = 120.0f;
        float period = 60.0f * 2.0f / bpm;
        mBpmAccumulator += dt;
        while (mBpmAccumulator >= period) {
            switchTo((mCurrentIndex + 1) % (int)mClips.size());
            mBpmAccumulator -= period;
        }
    }

    // Frame-update the active clip (lets video play forward).
    if (!mClips.empty() && mClips[mCurrentIndex].clip && mClips[mCurrentIndex].loaded) {
        mClips[mCurrentIndex].clip->frameUpdate(dt);
    }

    // Update outputs + ParamSpec mirrors
    auto& out = getOutputs();
    bool ready = !mClips.empty() && mClips[mCurrentIndex].loaded;
    if (out.count("material_ready")) out.at("material_ready")->setValue(ready ? 1.0f : 0.0f);
    if (out.count("current_index"))  out.at("current_index")->setValue((float)mCurrentIndex);

    if (ready && mClips[mCurrentIndex].clip) {
        auto* clip = mClips[mCurrentIndex].clip.get();
        float t = clip->getTime();
        if (out.count("time"))       out.at("time")->setValue(t);
        if (out.count("is_playing")) out.at("is_playing")->setValue(clip->isPlaying() ? 1.0f : 0.0f);
        // M4 — duration + progress via TheoraClip::getDuration() (forward du reader).
        float dur = clip->getDuration();
        if (out.count("duration")) out.at("duration")->setValue(dur);
        if (out.count("progress")) out.at("progress")->setValue(dur > 0.0f ? std::clamp(t / dur, 0.0f, 1.0f) : 0.0f);
    } else {
        if (out.count("duration")) out.at("duration")->setValue(0.0f);
        if (out.count("progress")) out.at("progress")->setValue(0.0f);
    }

    auto* nameMir = mSpec.getParam("current_name");
    if (nameMir) nameMir->stringVal = getCurrentName();
    auto* matMir = mSpec.getParam("material_out");
    if (matMir) matMir->stringVal = getCurrentMaterialName();

    fireUpdate();
}

void VideoLibraryNode::cleanup() {
    mClips.clear();
    mPrevClipsCsv.clear();
}

} // namespace bbfx
