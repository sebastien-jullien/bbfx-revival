#pragma once
#include "CommandManager.h"
#include <string>

namespace bbfx {

/// Undoable command for viewport gizmo transformations.
/// Stores before/after state of position, scale, rotation (9 floats each).
class TransformNodeCommand : public Command {
public:
    TransformNodeCommand(const std::string& nodeName,
                         const std::string& desc,
                         float px, float py, float pz,
                         float sx, float sy, float sz,
                         float rx, float ry, float rz,
                         float npx, float npy, float npz,
                         float nsx, float nsy, float nsz,
                         float nrx, float nry, float nrz);

    void execute() override;
    void undo() override;
    std::string description() const override { return mDesc; }

private:
    void applyValues(const float vals[9]);

    std::string mNodeName;
    std::string mDesc;
    float mOld[9];
    float mNew[9];
};

} // namespace bbfx
