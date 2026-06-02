#pragma once

#include "OggReader.h"
#include "TheoraSeekMap.h"
#include <theora/theoradec.h>
#include <string>

namespace bbfx {

class TheoraReader : public OggReader {
public:
    explicit TheoraReader(const std::string& filename);
    ~TheoraReader() override;

    void readHeaders();
    bool readFrame(th_ycbcr_buffer ycbcr);
    void seekToTime(float time);
    /// Lot AV.5 round 16 — seek to an EXACT frame index in the seekmap.
    /// Walks packets and counts non-dup decodes until reaching targetIdx.
    /// Avoids the linear-approximation drift of seekToTime that can land
    /// 1 frame off due to float precision in the kf_t + N×1/fps formula.
    void seekToFrameIndex(int targetIdx);
    /// Lot AV.5 round 17 — output the decoder's CURRENT frame to the provided
    /// ycbcr buffer. Used by setReverse to pre-fill the shared mYCbCr with the
    /// seek's landed frame, BEFORE producer restarts. This way consumer's first
    /// post-swap blit shows the EXACT mirror frame.
    bool getCurrentFrame(th_ycbcr_buffer ycbcr);
    void buildSeekMap();

    /// Lot AV.5 round 12 — temps RÉEL (via granule encodé) du dernier frame
    /// décodé par readFrame ou seekToTime. Permet à TheoraClip de tracker mTime
    /// fidèle au contenu source, indépendamment des dup frames qui faussaient
    /// l'incrément linéaire mTime += 1/fps (= les dups consomment un packet
    /// sans avancer "framesDecoded" mais font avancer le temps réel du stream).
    float getLastDecodedTime() const { return mLastDecodedTime; }

    /// Lot AV.5 round 14 — frame index courant (= index dans seekmap.mEntries
    /// du frame présent dans le buffer décodeur). Utilisé pour le mirror via
    /// frame index : forward frame M ↔ reverse frame (totalFrames - 1 - M).
    /// Évite la dérive due aux dup frames qui faussent la mapping `time`.
    int getCurrentFrameIndex() const { return mFrameIndex; }
    int getTotalFrameCount() const { return static_cast<int>(mSeekMap.size()); }
    const TheoraSeekMap& getSeekMap() const { return mSeekMap; }

    int getWidth() const;
    int getHeight() const;
    float getFrameRate() const;
    float getDuration() const;

private:
    void requireSeekMap();
    size_t getFileSize() const;

    th_info mInfo;
    th_comment mComment;
    th_setup_info* mSetup = nullptr;
    th_dec_ctx* mDecoder = nullptr;
    TheoraSeekMap mSeekMap;
    int mFrameCount = 0;
    bool mLogNextFrame = false;
    /// Round 12 — temps réel du dernier frame décodé.
    float mLastDecodedTime = 0.0f;
    /// Round 13 — index du frame actuellement dans le buffer décodeur (= index
    /// dans seekmap.mEntries). Utilisé pour obtenir le temps EXACT via le
    /// seekmap au lieu de l'estimation linéaire (kf_t + frames × 1/fps) qui
    /// dérive si des dup frames existent entre kf et target.
    int mFrameIndex = -1;
};

} // namespace bbfx
