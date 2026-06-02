#pragma once

#include "../../core/AnimationNode.h"
#include "../../core/ParamSpec.h"
#include "../../video/TheoraClip.h"
#include <memory>
#include <string>
#include <vector>

namespace bbfx {

/// VideoLibraryNode — switchable bank of N TheoraClip instances. Reproduces the
/// BBFx 2006 banque vidéo pattern (8 clips test1..test8, cf. setFanions.materials.lua).
///
/// Inputs (DAG):
///   - dt, index (int-as-float), next, prev, goto_index, play, pause, stop, seek (0..1)
/// Outputs (DAG):
///   - material_ready, time, progress, current_index, duration, is_playing
///
/// ParamSpec:
///   - clips (STRING — semicolon-separated filenames)
///   - play_mode (ENUM loop/once/ping_pong)
///   - bpm_sync (BOOL)
///   - current_name (STRING mirror, read-only)
///   - material_out (STRING mirror, read-only)
class VideoLibraryNode : public AnimationNode {
public:
    enum class PlayMode { Loop, Once, PingPong };

    explicit VideoLibraryNode(const std::string& name);
    ~VideoLibraryNode() override = default;

    void update() override;
    void cleanup() override;
    std::string getTypeName() const override { return "VideoLibraryNode"; }

    int  getCurrentIndex() const { return mCurrentIndex; }
    int  getClipCount()    const { return (int)mClips.size(); }
    const std::string& getCurrentName() const;
    const std::string& getCurrentMaterialName() const;
    bool isCurrentPlaying() const;
    PlayMode getPlayMode() const { return mPlayMode; }

private:
    struct ClipEntry {
        std::string filename;        // basename without extension
        std::string fullPath;        // path used to load (e.g. "video/foo.ogg")
        std::unique_ptr<TheoraClip> clip;
        bool loaded = false;
    };

    ParamSpec mSpec;
    std::vector<ClipEntry> mClips;
    int  mCurrentIndex = 0;
    PlayMode mPlayMode = PlayMode::Loop;
    bool mBpmSync = false;

    // Edge detection
    bool mPrevNextState = false;
    bool mPrevPrevState = false;
    bool mPrevPlayState = false;
    bool mPrevPauseState = false;
    bool mPrevStopState = false;
    int  mPrevGotoIndex = -1;
    int  mPrevIndexInput = -1;
    float mPrevSeek = -1.0f;

    // BPM accumulator
    float mBpmAccumulator = 0.0f;
    float mPrevDt = 1.0f / 60.0f;

    // Cached parsed clip list (semicolon-separated)
    std::string mPrevClipsCsv;

    void parseClipsIfChanged();
    void switchTo(int newIdx);
    static std::string basename(const std::string& path);
    static std::string parentDir(const std::string& path);
};

} // namespace bbfx
