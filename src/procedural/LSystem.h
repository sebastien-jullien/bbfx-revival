#pragma once

#include <map>
#include <string>
#include <vector>

namespace bbfx {

/// v3.5 Lot R — Lindenmayer-system string rewriter + 3D turtle
/// graphics mesh generator.
///
/// Supported symbols (turtle):
///   F        advance forward by `stepLength`, drawing a segment
///   f        advance forward without drawing
///   +        yaw  right by `angle`
///   -        yaw  left  by `angle`
///   &        pitch down by `angle`
///   ^        pitch up   by `angle`
///   /        roll  right by `angle`
///   \        roll  left  by `angle`
///   [  ]     push / pop the current turtle state
///   any other symbol is left unchanged
class LSystem {
public:
    struct Segment {
        float x0, y0, z0;
        float x1, y1, z1;
    };

    LSystem() = default;

    void setAxiom(const std::string& s) { mAxiom = s; }
    void addRule(char from, const std::string& to) { mRules[from] = to; }
    void clearRules() { mRules.clear(); }
    void setIterations(int n) { mIterations = n; }
    void setAngleDegrees(float d) { mAngleDeg = d; }
    void setStepLength(float s) { mStep = s; }

    /// Derive the final string from axiom + rules + iteration count.
    std::string derive() const;

    /// Run the turtle on the derived string and collect segments.
    std::vector<Segment> segments() const;

    /// Build an OGRE mesh (line list or thin cylinders) named
    /// `meshName` and return it. Empty on failure.
    std::string generateMesh(const std::string& meshName) const;

private:
    std::string mAxiom = "F";
    std::map<char, std::string> mRules;
    int   mIterations = 3;
    float mAngleDeg   = 25.7f;
    float mStep       = 1.0f;
};

} // namespace bbfx
