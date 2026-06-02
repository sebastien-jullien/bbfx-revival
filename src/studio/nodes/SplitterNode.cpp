#include "SplitterNode.h"
#include <algorithm>
#include <string>
namespace bbfx {

SplitterNode::SplitterNode(const std::string& name) : AnimationNode(name) {
    ParamDef numDef; numDef.name = "outputs"; numDef.label = "Outputs"; numDef.type = ParamType::INT;
    numDef.intVal = 4; numDef.minVal = 2; numDef.maxVal = 8;
    mSpec.addParam(numDef);
    setParamSpec(&mSpec);

    addInput(new AnimationPort("in", 0.0f));
    for (int i = 1; i <= mNumOutputs; i++)
        addOutput(new AnimationPort("out_" + std::to_string(i), 0.0f));
}

void SplitterNode::update() {
    // D3 — le param `outputs` pilote le nombre de sorties actives. Création des
    // ports en grow-only (préserve les liens existants, cf. convention ArtnetInput) ;
    // les sorties au-delà du compte courant sont mises à 0 (inertes mais conservées).
    int want = 4;
    if (auto* p = mSpec.getParam("outputs")) want = std::clamp(p->intVal, 2, 8);
    for (int i = mNumOutputs + 1; i <= want; ++i) {
        std::string n = "out_" + std::to_string(i);
        if (getOutputs().find(n) == getOutputs().end())
            addOutput(new AnimationPort(n, 0.0f));
    }
    if (want > mNumOutputs) mNumOutputs = want;

    float val = getInputs().at("in")->getValue();
    for (int i = 1; i <= mNumOutputs; ++i) {
        auto it = getOutputs().find("out_" + std::to_string(i));
        if (it != getOutputs().end())
            it->second->setValue(i <= want ? val : 0.0f);
    }
    fireUpdate();
}
} // namespace bbfx
