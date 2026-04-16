#pragma once
#include "../../core/AnimationNode.h"
#include "../../core/ParamSpec.h"
#include "../../midi/MidiClockGenerator.h"
#include <memory>

namespace bbfx {

/// DAG node that sends MIDI messages based on input port values.
/// When send_note transitions 0→1, sends note-on; 1→0 sends note-off.
/// When send_cc transitions 0→1, sends CC message.
///
/// Mode: "note_cc" (default) or "clock_master" (v3.4 Lot L).
/// In clock_master mode: generates MIDI Timing Clock (24 PPQN) at the configured BPM.
class MidiOutputNode : public AnimationNode {
public:
    explicit MidiOutputNode(const std::string& name);
    ~MidiOutputNode() override;
    void update() override;
    std::string getTypeName() const override { return "MidiOutputNode"; }

    /// Clock master helpers (also callable from dbg commands).
    void clockStart(float bpm);
    void clockStop();
    bool isClockRunning() const;
    float getClockBpm() const;

private:
    ParamSpec mSpec;
    float mLastSendNote = 0.0f;
    float mLastSendCC = 0.0f;
    float mLastLedNote = -1.0f;
    float mLastLedVel = -1.0f;

    // ── Clock master (v3.4 Lot L) ─────────────────────────────────────
    std::unique_ptr<MidiClockGenerator> mClockGen;
    float mLastClockBpm = 0.0f; ///< BPM value from last frame (for change detection)
};

} // namespace bbfx
