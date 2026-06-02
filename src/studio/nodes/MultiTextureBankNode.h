#pragma once

#include "../../core/AnimationNode.h"
#include "../../core/ParamSpec.h"
#include <random>
#include <string>
#include <vector>

namespace bbfx {

/// MultiTextureBankNode — indexed 2D bank of texture presets (each preset
/// holds N slots, typically a {gray, color} pair). Direct equivalent of the
/// BBFx 2006 `mux()` Lua helper (cf. setFanions.textures.lua:233-246).
///
/// Encoding of the `presets` ParamSpec string: rows separated by ';',
/// columns separated by '|'. E.g. "a|b;c|d;e|f" = 3 presets x 2 slots.
class MultiTextureBankNode : public AnimationNode {
public:
    enum class Mode { Sequential, Random, BpmSynced };

    explicit MultiTextureBankNode(const std::string& name);
    ~MultiTextureBankNode() override = default;

    void update() override;
    std::string getTypeName() const override { return "MultiTextureBankNode"; }

    int  getPresetIndex() const { return mPresetIndex; }
    int  getPresetCount() const { return (int)mPresets.size(); }
    int  getSlotCount()   const { return mSlotCount; }
    Mode getMode()        const { return mMode; }
    const std::string& getSlotTexture(int slot) const;

private:
    ParamSpec mSpec;
    std::vector<std::vector<std::string>> mPresets;
    int  mPresetIndex = 0;
    int  mSlotCount = 2;
    Mode mMode = Mode::Sequential;

    std::string mPrevPresetsCsv;
    int  mPrevSlotCount = -1;

    bool mPrevNextState = false;
    bool mPrevPrevState = false;
    int  mPrevGotoIndex = -1;
    int  mPrevPresetIndexInput = -1;

    float mBpmAccumulator = 0.0f;
    float mPrevDt = 1.0f / 60.0f;

    std::mt19937 mRng;
    bool  mRngInit = false;

    void parsePresetsIfChanged();
    void seedRngIfNeeded();
    int  pickRandomNext();
    void writeMirrors();
};

} // namespace bbfx
