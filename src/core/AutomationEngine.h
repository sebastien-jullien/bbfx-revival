#pragma once

namespace bbfx {

class AutomationData;
class Animator;

class AutomationEngine {
public:
    explicit AutomationEngine(AutomationData* data);

    /// Evaluate all non-muted lanes and inject interpolated values into DAG ports.
    void evaluate(float currentBeat, Animator* animator);

    void setEnabled(bool enabled) { mEnabled = enabled; }
    bool isEnabled() const { return mEnabled; }

private:
    AutomationData* mData;
    bool mEnabled = true;
};

} // namespace bbfx
