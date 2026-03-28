#pragma once

#include "../bbfx.h"
#include "AnimationPort.h"
#include <map>

namespace bbfx {

class AnimationNode; // forward

class AnimationNodeListener {
public:
    virtual ~AnimationNodeListener() = default;
    virtual void notifyUpdate(AnimationNode* node) = 0;
};

class AnimationNode {
public:
    AnimationNode();
    explicit AnimationNode(const string& name);
    virtual ~AnimationNode();

    const string& getName() const;
    virtual std::string getTypeName() const { return "AnimationNode"; }

    using Ports = std::map<string, AnimationPort*>;
    const Ports& getInputs() const;
    const Ports& getOutputs() const;

    virtual void update();

    void setListener(AnimationNodeListener* listener);

protected:
    string mName;
    Ports mInputs;
    Ports mOutputs;

    AnimationPort* addInput(AnimationPort* port);
    AnimationPort* addOutput(AnimationPort* port);
    void fireUpdate();

private:
    AnimationNodeListener* mListener = nullptr;
    void addPort(AnimationPort* port, Ports& ports);
    void setFullName(AnimationPort* port) const;
    void destroyPorts(Ports& ports);
};

} // namespace bbfx
