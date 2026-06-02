#include "TheoraClipNode.h"
#include "../core/DebugLog.h"
#include <cmath>
#include <iostream>

namespace bbfx {

TheoraClipNode::TheoraClipNode(const std::string& filename)
    : TheoraClipNode("TheoraClipNode", filename) {}

TheoraClipNode::TheoraClipNode(const std::string& name, const std::string& filename)
    : AnimationNode(name)
    , mClip(std::make_unique<TheoraClip>(filename, name))   // pass node name as instanceTag → unique OGRE resource names
{
    addInput(new AnimationPort("dt", 0.016f));
    addInput(new AnimationPort("time_control", 0.0f));
    addOutput(new AnimationPort("playing", 0.0f));
    addOutput(new AnimationPort("frame_ready", 0.0f));

    // v3.5.2 Lot V — expose mClip->getMaterialName() so MaterialBridgeNode (Lot T)
    // can route this video onto a 3D mesh. The mirror is initially populated
    // with whatever the blitter named the material at construction; update()
    // refreshes it each frame to handle late-init cases.
    ParamDef matOutDef;
    matOutDef.name = "material_out";
    matOutDef.label = "Material (out)";
    matOutDef.type  = ParamType::STRING;
    matOutDef.stringVal = mClip ? mClip->getMaterialName() : std::string();
    matOutDef.readOnly = true;
    matOutDef.tooltip = "Read-only: live material name from the Theora blitter. "
                        "Consumable by MaterialBridgeNode (Sprint S5 Lot T).";
    mSpec.addParam(matOutDef);
    setParamSpec(&mSpec);

    addOutput(new AnimationPort("material_ready", 0.0f));
    // v3.5.2 Sprint S8 Lot AU.7/AU.9 — `play` (>= 0.5 → playing, default 1.0),
    // `loop` (>= 0.5 → loop, default 1.0), `speed` (playback rate multiplier,
    // default 1.0 ; 0 → pause, 2 → 2× fast-forward).
    // Lot AV.4 round 4 — `reverse` (>= 0.5 → joue le clip pré-rendu inversé,
    // chargé via le param ParamSpec `reverse_filename`). Reproduction 2006
    // `ReversableClip::doReverse()` : swap atomique du pointeur sur le 2ᵉ
    // TheoraReader sans paralléliser un 2ᵉ décodage.
    addInput(new AnimationPort("play",    1.0f));
    addInput(new AnimationPort("loop",    1.0f));
    addInput(new AnimationPort("speed",   1.0f));
    addInput(new AnimationPort("reverse", 0.0f));

    ParamDef revFileDef;
    revFileDef.name = "reverse_filename"; revFileDef.label = "Reverse file";
    revFileDef.type  = ParamType::STRING;
    revFileDef.stringVal = "";
    revFileDef.tooltip = "Lot AV.4 round 4 — chemin vers le 2ᵉ Theora stream pré-rendu inversé "
                         "(typiquement ffmpeg -i clip.ogg -vf reverse clip_reverse.ogg). "
                         "Vide = pas de reverse possible. Doit avoir mêmes dimensions/framerate que le forward.";
    mSpec.addParam(revFileDef);

    setPortTooltip("dt", "Time delta from RootTimeNode — auto-driven, normally not user-set.");
    setPortTooltip("play", "Playback gate: >= 0.5 plays (default), < 0.5 pauses. Wire a trigger/joystick toggle to control it.");
    setPortTooltip("loop", "Loop gate: >= 0.5 loops the clip (default), < 0.5 plays once.");
    setPortTooltip("speed", "Playback rate (default 1.0). 0 = pause, 2 = fast-forward 2×, 0.5 = slow-motion.");
    setPortTooltip("reverse", "Lot AV.4 round 4 — >= 0.5 active le 2ᵉ Theora stream pré-rendu inversé "
                              "(param `reverse_filename`). Swap atomique du reader, pas de double décodage. "
                              "Reproduction 2006 ReversableClip.");
    setPortTooltip("playing",
        "1.0 while clip is playing, 0.0 when paused/stopped. Useful for triggering downstream nodes.");
    setPortTooltip("frame_ready", "Pulses 1.0 each new decoded frame is ready to be displayed.");
    setPortTooltip("material_ready",
        "Niveau 1.0 tant que le matériau est valide ET que le clip joue. "
        "Link to MaterialBridgeNode.material_source to route video onto a mesh.");
}

TheoraClipNode::~TheoraClipNode() = default;

void TheoraClipNode::update() {
    auto& inputs = getInputs();
    auto& outputs = getOutputs();

    // v3.5.2 Sprint S8 Lot AU.7 — drive playback from the `play`/`loop` ports.
    // `play` defaults to 1.0 and mLastPlayVal starts at 0 → the first update sees
    // a rising edge and starts playback (looping by default). Edge-detection (not
    // a level check) so a clip that ends without looping stays stopped instead of
    // tight-looping play()/EOF.
    if (mClip) {
        float loopVal = 1.0f;
        if (auto it = inputs.find("loop"); it != inputs.end()) loopVal = it->second->getValue();
        mClip->setLoop(loopVal >= 0.5f);

        float playVal = 1.0f;
        if (auto it = inputs.find("play"); it != inputs.end()) playVal = it->second->getValue();
        const bool wantPlay = playVal >= 0.5f;
        const bool wasPlay  = mLastPlayVal >= 0.5f;
        if (wantPlay && !wasPlay)      mClip->play();   // rising edge → (re)start from current pos
        else if (!wantPlay && wasPlay) mClip->pause();  // falling edge → pause
        mLastPlayVal = playVal;
    }

    // Lot AV.4 round 4 — charger le 2ᵉ Theora stream pré-rendu inversé si le param
    // `reverse_filename` est non-vide ET n'a pas déjà été chargé. Lazy-load.
    auto* revParam = mSpec.getParam("reverse_filename");
    if (revParam && !revParam->stringVal.empty()
     && revParam->stringVal != mLoadedReverseFile && mClip) {
        mClip->loadReverseStream(revParam->stringVal);
        mLoadedReverseFile = revParam->stringVal;
    }

    // Lot AV.4 round 4 — edge-detect le port `reverse` et basculer le reader actif.
    if (auto it = inputs.find("reverse"); it != inputs.end() && mClip) {
        const bool wantReverse = it->second->getValue() >= 0.5f;
        if (wantReverse != mLastReverseVal) {
            mClip->setReverse(wantReverse);
            mLastReverseVal = wantReverse;
        }
    }

    // Lot AV.5 round 16 — `time_control` port : si la valeur change, on appelle
    // setTime sur le clip. Sert pour les tests automatisés (dbg.set("clip",
    // "time_control", T)). En usage normal, on laisse à 0 et le clip joue
    // naturellement.
    if (auto it = inputs.find("time_control"); it != inputs.end() && mClip) {
        const float t = it->second->getValue();
        if (t > 0.0f && std::abs(t - mLastTimeControl) > 0.001f) {
            mClip->setTime(t);
            mLastTimeControl = t;
        }
    }

    float dt = inputs.at("dt")->getValue();
    float speed = 1.0f;
    if (auto it = inputs.find("speed"); it != inputs.end()) speed = it->second->getValue();
    if (speed < 0.0f) speed = 0.0f;   // negative speed via `reverse` port instead
    if (std::abs(speed - mLastSpeed) > 0.05f) {
        BBFX_DLOG("[theora_clip_node] speed change: " << mLastSpeed << " -> " << speed);
        mLastSpeed = speed;
    }
    // Lot AV.5 round 3 — modèle producer-paced : speed est envoyé au decode thread
    // qui se pace lui-même. dt est passé pour l'accumulation mTime (séparée de
    // la cadence de décodage).
    mClip->setTargetSpeed(speed);
    mClip->frameUpdate(dt);

    outputs.at("playing")->setValue(mClip->isPlaying() ? 1.0f : 0.0f);
    outputs.at("frame_ready")->setValue(mClip->isFrameReady() ? 1.0f : 0.0f);

    // Lot V: refresh mirror + material_ready output.
    const std::string& matName = mClip ? mClip->getMaterialName() : std::string();
    auto* outMir = mSpec.getParam("material_out");
    if (outMir) outMir->stringVal = matName;
    auto itReady = outputs.find("material_ready");
    if (itReady != outputs.end()) {
        const bool ready = !matName.empty() && mClip && mClip->isPlaying();
        itReady->second->setValue(ready ? 1.0f : 0.0f);
    }

    fireUpdate();
}

} // namespace bbfx
