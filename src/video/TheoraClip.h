#pragma once

#include "TheoraReader.h"
#include "TheoraBlitter.h"
#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <chrono>
#include <string>
#include <memory>

namespace bbfx {

class TheoraClip {
public:
    /// Construct a clip from a Theora file. The optional `instanceTag` is
    /// suffixed to the OGRE texture/material names so multiple clips can
    /// share the same source file without colliding in OGRE's resource
    /// managers (otherwise `theora_<sanitized_path>` is identical and
    /// `TextureManager::createManual` raises ItemIdentityException).
    explicit TheoraClip(const std::string& filename, const std::string& instanceTag = "");
    virtual ~TheoraClip();

    void play();
    void pause();
    void stop();
    void frameUpdate(float dt);

    void setTime(float t);
    float getTime() const { return mTime; }
    bool isPlaying() const { return mPlaying.load(); }
    bool isFrameReady() const { return mFrameReady.load(); }
    void setLoop(bool loop) { mLoop.store(loop); }
    bool isLooping() const { return mLoop.load(); }

    const std::string& getMaterialName() const { return mBlitter->getMaterialName(); }
    const std::string& getTextureName()  const { return mBlitter->getTextureName(); }
    int getWidth() const { return mReader->getWidth(); }
    int getHeight() const { return mReader->getHeight(); }

    /// Lot AV.4 round 4 (I-2056) — repro 2006 `ReversableClip::doReverse()`
    /// (cf. prog/workspace/test-theora/ReversableClip.cpp). Charge un 2ᵉ
    /// TheoraReader sur un fichier pré-rendu inversé (typiquement généré via
    /// `ffmpeg -i clip.ogg -vf reverse clip_reverse.ogg`). Au `setReverse(true)`,
    /// on swap atomiquement le pointeur `mReader` entre forward et reverse —
    /// UN SEUL thread de décodage actif, pas de parallel decode (vs 3-4 fps
    /// constatés avec 2 TheoraClipNode + VideoCrossfade en Lot AV.4 round 2).
    /// La vidéo paraît jouer en arrière car le clip B est physiquement inversé.
    void loadReverseStream(const std::string& reverseFilename);
    void setReverse(bool reverse);
    bool isReversed() const { return mReverseActive.load(); }
    bool hasReverseStream() const { return mReverseReader != nullptr; }

    /// Lot AV.5 round 3 (I-2056) — modèle producer-paced. Le decode thread se pace
    /// lui-même à `target_speed × native_fps` via sleep_until, et écrase mYCbCr en
    /// continu. Le consumer (frameUpdate) prend juste la frame courante quand
    /// mFrameReady est true. Speed > 1 → producer décode N× plus vite → contenu
    /// avance N× par seconde réelle. Speed=0 ou négatif → producer sleep (pause).
    void setTargetSpeed(float speed) { mTargetSpeed.store(speed); }
    float getTargetSpeed() const { return mTargetSpeed.load(); }

    /// Lot AV.5 round 29 (I-2056) — diagnostic getters pour le POC frame-mirror.
    /// Permettent à `dbg.video_*` (lua) de mesurer objectivement la dérive du
    /// mirror sur N swaps successifs : on compare l'index forward de la frame
    /// blittée AVANT swap vs AVANT swap suivant — devrait être 0 sans dérive.
    /// Tous les indices sont en "coord forward" (= traduit depuis reverseReader
    /// si mReverseActive : totalFrames - 1 - revIdx).
    int   getLastBlittedFwdIndex() const { return mLastBlittedFwdFrameIndex; }
    int   getTotalFrameCount()    const { return mReader ? mReader->getTotalFrameCount() : 0; }
    /// Durée totale du clip en secondes (forward = référence). 0 si pas de reader.
    /// Utilisé par VideoSlicerNode (mapping in/out → secondes) et VideoLibraryNode
    /// (sortie `duration` + `progress = time/duration`).
    float getDuration()           const { return mReader ? mReader->getDuration() : 0.0f; }
    float getLastBlittedTime()    const { return mLastBlittedTime; }
    /// Index forward courant du reader actif (= dernière frame DÉCODÉE par le
    /// producer, qui peut être en avance sur la dernière BLITTÉE en latest-wins).
    int   getCurrentFwdIndex() const {
        if (mReverseActive.load() && mReverseReader) {
            int revIdx = mReverseReader->getCurrentFrameIndex();
            int total  = mReader ? mReader->getTotalFrameCount() : 0;
            return (revIdx >= 0 && total > 0) ? (total - 1 - revIdx) : -1;
        }
        return mReader ? mReader->getCurrentFrameIndex() : -1;
    }

protected:
    void run(std::stop_token stopToken);
    virtual float getLoopStart() const { return 0.0f; }

    /// Forward reader — owns the forward Theora stream. Always non-null after construction.
    std::unique_ptr<TheoraReader> mReader;
    /// Reverse reader optionnel — owns the reversed Theora stream (chargé via
    /// `loadReverseStream`). Null si pas de reverse configuré.
    std::unique_ptr<TheoraReader> mReverseReader;
    /// Reverse mode actif. État miroir pour requêtes externes.
    std::atomic<bool> mReverseActive{false};
    std::unique_ptr<TheoraBlitter> mBlitter;

    /// Helper used by the decode thread to pick the active reader between the
    /// forward and reverse streams. Returns a raw pointer that is safe under
    /// the `mFrameMutex` lock (acquired around readFrame/seekToTime/swap).
    TheoraReader* activeReader() {
        return mReverseActive.load() && mReverseReader ? mReverseReader.get() : mReader.get();
    }

    std::jthread mThread;
    std::mutex mFrameMutex;
    std::mutex mTimeMutex;
    std::condition_variable mFrameCond;

    std::atomic<bool> mPlaying{false};
    std::atomic<bool> mRunning{false};
    std::atomic<bool> mFrameReady{false};
    std::atomic<bool> mSeeking{false};
    std::atomic<bool> mLoop{false};
    std::atomic<float> mTargetSpeed{1.0f};   // Lot AV.5 round 3 — pace producer
    /// Lot AV.5 round 17 — set by setReverse after pre-filling mYCbCr with the
    /// seek's mirror frame. Producer's first decode-iter checks this : if true,
    /// waits for consumer's first blit (= mFrameReady → false) before decoding
    /// next frame. Guarantees consumer's first post-swap blit = exact mirror.
    std::atomic<bool> mSwapPending{false};
    /// Round 18 diag — count consumer blits since last setReverse for log purposes.
    std::atomic<int> mPostSwapBlitCount{0};

    /// Lot AV.5 round 9 — chrono-based estimation of current source position.
    /// Used by `setReverse` to compute the mirror seek, robust against rapid swaps
    /// or slow producer/consumer. The estimate is `mLastSeekSourcePos + (now -
    /// mLastSeekWallTime) × speed`, modulo duration. Recorded on every successful
    /// seek (setTime, setReverse, loop rollover).
    std::chrono::steady_clock::time_point mLastSeekWallTime{};
    float mLastSeekSourcePos{0.0f};

    // Lot AV.5 round 13 — mForwardSavedPos removed. Pure mirror at each swap;
    // no tape-recorder logic. Orbit in symmetric timing is the inherent and
    // accepted behavior ("vrai reverse exact").

    /// Lot AV.5 round 15 — frame index (en coordonnées FORWARD stream) du
    /// frame que le CONSUMER a effectivement posé à l'écran lors du dernier
    /// blit. C'est la VRAIE position que l'user voit, indépendamment de
    /// combien de frames le producer a décodé en avance dans mYCbCr (= peut
    /// arriver si consumer est plus lent que producer).
    int mLastBlittedFwdFrameIndex = -1;
    /// Lot AV.5 round 20+30 — RING BUFFER of recent blit frame indices +
    /// timestamps. setReverse uses an entry from "N ms ago" (= compensates for
    /// input latency between user's physical press and setReverse fire). User's
    /// eye sees the frame at the LAST OGRE render, but by the time setReverse
    /// fires (SDL polling + BFS tick later), the rendered frame has advanced.
    /// Looking up the OLDER value gives the frame at the moment of press.
    ///
    /// Two compensation modes (round 30) :
    ///   - Tick-based (legacy round 20) : lookup N entries back in the ring.
    ///     Coupled to BFS tick rate ; less robust to render rate variations.
    ///   - Wall-time-based (round 30) : lookup the most recent entry older than
    ///     (now − N ms). Robust to BFS rate variations. Default mode.
    ///
    /// Env var overrides at construction :
    ///   BBFX_LATENCY_FRAMES=N   → tick-based mode with N frames lookback.
    ///   BBFX_LATENCY_MS=N       → wall-time mode with N ms lookback (default 50).
    ///   BBFX_LATENCY_FRAMES=0   → disable compensation entirely.
    static constexpr int RING_SIZE = 32;
    int mBlittedFwdRingBuffer[RING_SIZE]{};
    std::chrono::steady_clock::time_point mBlittedFwdRingTime[RING_SIZE]{};
    int mBlittedFwdRingHead = 0;
    /// Round 30 — mode selection : if mLatencyMode == "ticks", use tick-based
    /// lookup ; else wall-time. Default "ms" (= 50ms wall-time).
    enum class LatencyMode { Ticks, Ms };
    /// Round 31 — default = TICKS / 0 = NO COMPENSATION. C'est le comportement
    /// "mirror sur la frame courante exactement visible". User a clarifié qu'il
    /// préfère pas de compensation et pas de saut visuel au swap, plutôt qu'une
    /// compensation qui crée un saut. Tunable via env vars pour expérimentation.
    LatencyMode mLatencyMode = LatencyMode::Ticks;
    int mLatencyCompensationFrames = 0;   // default 0 = no compensation
    int mLatencyCompensationMs     = 50;  // unused unless mode = Ms via env

    float mTime = 0.0f;
    /// Lot AV.5 round 5 — position de la dernière frame BLITTÉE (= ce que voit
    /// l'utilisateur à l'écran). Différente de mTime (dernière frame DÉCODÉE)
    /// quand le producer va plus vite que le consumer (latest-wins drop). Le
    /// setReverse mirror utilise mLastBlittedTime pour que le swap aligne sur
    /// le frame visible, pas sur le frame qu'on vient de décoder en backlog.
    float mLastBlittedTime = 0.0f;
    float mDecodeTime = 0.0f;
    float mFrameTimer = 0.0f;  // accumulator for frame pacing
    th_ycbcr_buffer mYCbCr;
    int mFramesRendered = 0;
    int mFramesDropped = 0;
};

} // namespace bbfx
