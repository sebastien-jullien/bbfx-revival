#include "BeatTriggerNode.h"
#include <cmath>
#include <algorithm>
#include <string>
namespace bbfx {

BeatTriggerNode::BeatTriggerNode(const std::string& name) : AnimationNode(name) {
    ParamDef subDef; subDef.name = "subdivision"; subDef.label = "Subdivision"; subDef.type = ParamType::ENUM;
    subDef.stringVal = "beat"; subDef.choices = {"beat","half","quarter","eighth","bar","cycle"};
    mSpec.addParam(subDef);
    ParamDef atkDef; atkDef.name = "attack"; atkDef.label = "Attack"; atkDef.type = ParamType::FLOAT;
    atkDef.floatVal = 0.02f; atkDef.minVal = 0.001f; atkDef.maxVal = 0.5f;
    mSpec.addParam(atkDef);
    ParamDef decDef; decDef.name = "decay"; decDef.label = "Decay"; decDef.type = ParamType::FLOAT;
    decDef.floatVal = 0.15f; decDef.minVal = 0.01f; decDef.maxVal = 2.0f;
    mSpec.addParam(decDef);
    ParamDef intDef; intDef.name = "intensity"; intDef.label = "Intensity"; intDef.type = ParamType::FLOAT;
    intDef.floatVal = 1.0f; intDef.minVal = 0; intDef.maxVal = 5;
    mSpec.addParam(intDef);
    setParamSpec(&mSpec);

    addInput(new AnimationPort("beat", 0.0f));
    addInput(new AnimationPort("beatFrac", 0.0f));
    addInput(new AnimationPort("dt", 0.016f));
    addOutput(new AnimationPort("trigger", 0.0f));
    addOutput(new AnimationPort("envelope", 0.0f));
    addOutput(new AnimationPort("phase", 0.0f));
}

void BeatTriggerNode::update() {
    auto& in = getInputs();
    auto& out = getOutputs();
    float beat = in.at("beat")->getValue();
    float beatFrac = in.at("beatFrac")->getValue();
    float dt = in.at("dt")->getValue();

    // D2 — subdivision : multiplicateur appliqué au compteur de beats. On déclenche
    // quand l'entier de (beat * mult) augmente. mult>1 = subdivisions plus fines que
    // le beat, mult<1 = groupes (bar/cycle).
    //   cycle(1/16) < bar(1/4) < beat(1) < half(2) < quarter(4) < eighth(8)
    float mult = 1.0f;
    if (auto* p = mSpec.getParam("subdivision")) {
        const std::string& s = p->stringVal;
        if      (s == "half")    mult = 2.0f;
        else if (s == "quarter") mult = 4.0f;
        else if (s == "eighth")  mult = 8.0f;
        else if (s == "bar")     mult = 0.25f;
        else if (s == "cycle")   mult = 0.0625f;
        else                     mult = 1.0f; // "beat"
    }
    float subBeat = std::floor(beat * mult);
    bool triggered = (subBeat > mLastSubBeat && mLastSubBeat >= 0.0f);
    mLastSubBeat = subBeat;

    // Envelope: attack ramp then decay (D2 — `attack` était ignoré : saut instantané).
    // Null-checks getParam : ne pas déréférencer un param potentiellement absent
    // (projet/preset corrompu) — fallback sur les défauts du ParamSpec.
    auto* pAtk = mSpec.getParam("attack");
    auto* pDec = mSpec.getParam("decay");
    auto* pInt = mSpec.getParam("intensity");
    float attack = pAtk ? pAtk->floatVal : 0.02f;
    float decay = pDec ? pDec->floatVal : 0.15f;
    float intensity = pInt ? pInt->floatVal : 1.0f;

    if (triggered) {
        mAttacking = true; // arme la rampe d'attaque
    }
    if (mAttacking) {
        // monte vers intensity sur `attack` secondes
        mEnvelope += (intensity / std::max(0.001f, attack)) * dt;
        if (mEnvelope >= intensity) { mEnvelope = intensity; mAttacking = false; }
    } else if (mEnvelope > 0.0f) {
        mEnvelope -= dt / std::max(0.001f, decay);   // garde div/0 (decay=0 → inf/NaN), parité avec attack
        if (mEnvelope < 0.0f) mEnvelope = 0.0f;
    }

    out.at("trigger")->setValue(triggered ? 1.0f : 0.0f);
    out.at("envelope")->setValue(mEnvelope);
    out.at("phase")->setValue(beatFrac);
    fireUpdate();
}

} // namespace bbfx
