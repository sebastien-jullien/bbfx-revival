#include "TheoraClip.h"
#include <cmath>
#include <iostream>
#include <chrono>
#include <algorithm>
#include <cstdlib>

namespace bbfx {

using Clock = std::chrono::steady_clock;

TheoraClip::TheoraClip(const std::string& filename, const std::string& instanceTag) {
    mLastSeekWallTime = std::chrono::steady_clock::now();
    mLastSeekSourcePos = 0.0f;
    // Lot AV.5 round 30 — ring buffer initialisé à -1 (= "pas encore blitté")
    // pour distinguer une entrée non écrite d'un index frame valide 0.
    std::fill(std::begin(mBlittedFwdRingBuffer), std::end(mBlittedFwdRingBuffer), -1);
    // Sélection du mode de compensation latence via env vars (tunable POC) :
    //   BBFX_LATENCY_FRAMES=N → mode tick-based (lookup N entries back)
    //   BBFX_LATENCY_MS=N     → mode wall-time (lookup most recent entry > N ms ago)
    //   Default = mode wall-time, 50 ms (≈ 2-3 BFS ticks à 30-60 fps render)
    // Si les deux env vars sont set, frames a priorité (= mode explicite).
    if (const char* env = std::getenv("BBFX_LATENCY_FRAMES")) {
        int v = std::atoi(env);
        if (v >= 0 && v < RING_SIZE) {
            mLatencyMode = LatencyMode::Ticks;
            mLatencyCompensationFrames = v;
            std::cout << "[theora_clip] BBFX_LATENCY_FRAMES override: " << v
                      << " (mode=ticks)" << std::endl;
        }
    } else if (const char* envMs = std::getenv("BBFX_LATENCY_MS")) {
        int v = std::atoi(envMs);
        if (v >= 0 && v < 5000) {
            mLatencyMode = LatencyMode::Ms;
            mLatencyCompensationMs = v;
            std::cout << "[theora_clip] BBFX_LATENCY_MS override: " << v
                      << " (mode=wall-time)" << std::endl;
        }
    }
    mReader = std::make_unique<TheoraReader>(filename);
    mReader->readHeaders();

    mBlitter = std::make_unique<TheoraBlitter>();
    std::string texName = "theora_" + filename;
    // Clean texture name (path separators and dots → underscores)
    for (auto& c : texName) {
        if (c == '/' || c == '\\' || c == '.') c = '_';
    }
    // v3.5.2 Lot V cleanup: append instance tag so multiple clips can share
    // the same source file without colliding in OGRE's resource managers.
    if (!instanceTag.empty()) {
        std::string sanitized = instanceTag;
        for (auto& c : sanitized) {
            if (c == '/' || c == '\\' || c == '.') c = '_';
        }
        texName += "_" + sanitized;
    }
    mBlitter->setup(texName, mReader->getWidth(), mReader->getHeight());

    std::cout << "[theora_clip] Loaded: " << filename
              << " (" << mReader->getWidth() << "x" << mReader->getHeight() << ")" << std::endl;
}

TheoraClip::~TheoraClip() {
    stop();
}

void TheoraClip::play() {
    if (mPlaying.load()) return;

    mFrameTimer = 0.0f;

    // If thread is still running (e.g. after pause), just resume it
    if (mRunning.load() && mThread.joinable()) {
        mPlaying.store(true);
        mFrameCond.notify_all();
        std::cout << "[theora_clip] Playing" << std::endl;
        return;
    }

    // Fresh start (first play or after stop/EOF)
    mPlaying.store(true);
    mRunning.store(true);
    // Lot AV.5 round 32 — préserver mFrameReady SSI un swap est en attente.
    // Scénario : `setReverse()` avec wasPlaying=false (= clip en pause au moment
    // du swap) pré-décode la mirror frame dans mYCbCr, set mFrameReady=true et
    // mSwapPending=true, mais ne rappelle PAS play() (wasPlaying=false).
    // Quand l'user un-pause ensuite, play() (fresh-start path) écraserait
    // mFrameReady=false → le consumer ne blit jamais la mirror frame → le
    // producer thread (lancé ci-dessous) reste bloqué en wait sur mSwapPending
    // qui n'est jamais cleared (par le clear post-blit). DEADLOCK : l'user
    // doit re-cliquer reverse pour débloquer.
    // Fix : si mSwapPending est armé, mFrameReady doit rester true.
    if (!mSwapPending.load()) {
        mFrameReady.store(false);
    }
    mThread = std::jthread([this](std::stop_token st) { run(st); });
    std::cout << "[theora_clip] Playing" << std::endl;
}

void TheoraClip::pause() {
    mPlaying.store(false);
    mFrameCond.notify_all();  // unblock decode thread if waiting
    std::cout << "[theora_clip] Paused" << std::endl;
}

void TheoraClip::stop() {
    mPlaying.store(false);
    mRunning.store(false);
    mFrameCond.notify_all();

    if (mThread.joinable()) {
        mThread.request_stop();
        mThread.join();
    }

    // Reset position so next play() restarts from beginning
    {
        std::lock_guard lock(mTimeMutex);
        mTime = 0.0f;
        mLastBlittedTime = 0.0f;
        mFrameTimer = 0.0f;
        mLastSeekWallTime = std::chrono::steady_clock::now();
        mLastSeekSourcePos = 0.0f;
    }
    // Reset both readers to 0 so a future swap (setReverse) is also at start.
    mReader->seekToTime(0.0f);
    if (mReverseReader) mReverseReader->seekToTime(0.0f);
    mFrameReady.store(false);
    std::cout << "[theora_clip] Stopped (reset to start)" << std::endl;
}

void TheoraClip::run(std::stop_token stopToken) {
    // Lot AV.5 round 3 (I-2056) — modèle producer-paced (latest-wins).
    //
    // Au lieu de l'alternance producer/consumer (qui plafonnait à 1 frame/render),
    // le producer décode à un rythme `target_speed × native_fps` géré par
    // sleep_until. Le consumer (frameUpdate) prend simplement la frame courante
    // de mYCbCr quand mFrameReady=true. Si le consumer est plus lent que le
    // producer, des frames intermédiaires sont écrasées : c'est le comportement
    // attendu d'un fast-forward (= un vrai lecteur vidéo). Le producer ne bloque
    // PLUS sur le consumer, donc speed multiplier marche jusqu'à la limite du
    // débit décodage Theora.
    //
    // Decode latency Theora ≈ 1ms (Debug) / 0.2ms (Release). Donc max throughput
    // producer ≈ 1000 fps (Debug) / 5000 fps (Release). Cap pratique = display
    // rate × speed.

    using namespace std::chrono;
    auto lastDecode = Clock::now();
    const float fps = mReader ? mReader->getFrameRate() : 30.0f;

    while (!stopToken.stop_requested() && mRunning.load()) {
        if (!mPlaying.load() || mSeeking.load()) {
            std::this_thread::sleep_for(milliseconds(10));
            lastDecode = Clock::now();
            continue;
        }

        // Lot AV.5 round 29 (round 7 réintroduit) — wait for the consumer to
        // blit the pre-decoded mirror frame BEFORE producing the next one.
        // setReverse() pre-decodes mYCbCr and sets mSwapPending=true. The
        // consumer's frameUpdate() clears mSwapPending+notify after its first
        // post-swap blit. This eliminates the consumer-lag race that, before
        // this guard, let the producer overwrite mYCbCr with frame mirror+1,2,3
        // before the consumer had a chance to display the mirror frame itself.
        if (mSwapPending.load()) {
            std::unique_lock<std::mutex> lock(mFrameMutex);
            // Bounded wait — re-check every 50ms in case consumer is slow
            // (Studio render lag). Don't deadlock if mRunning flips.
            mFrameCond.wait_for(lock, milliseconds(50), [this, &stopToken]() {
                return !mSwapPending.load()
                    || stopToken.stop_requested()
                    || !mRunning.load();
            });
            if (stopToken.stop_requested() || !mRunning.load()) break;
            if (mSwapPending.load()) {
                // Still pending → consumer hasn't ticked yet. Loop back to
                // re-check (not break) so we don't burn CPU.
                continue;
            }
            // Cleared → re-seed lastDecode so the very next decode isn't paced
            // "from before the swap" (= which would burst-decode).
            lastDecode = Clock::now();
        }


        // Pace decode based on target speed.
        float speed = mTargetSpeed.load();
        if (speed < 0.01f) {
            // Treat near-zero as pause-like : sleep briefly, don't decode.
            std::this_thread::sleep_for(milliseconds(20));
            lastDecode = Clock::now();
            continue;
        }
        auto targetInterval = microseconds(static_cast<int64_t>(1e6f / (fps * speed)));
        auto nextDecodeTime = lastDecode + targetInterval;
        auto now = Clock::now();
        if (now < nextDecodeTime) {
            // Use sleep_for with bounded duration to remain reactive to stop.
            auto remaining = duration_cast<milliseconds>(nextDecodeTime - now);
            if (remaining.count() > 20) remaining = milliseconds(20);
            std::this_thread::sleep_for(remaining);
            continue;
        }
        lastDecode = now;

        {
            std::lock_guard lock(mFrameMutex);
            if (!mPlaying.load() || mSeeking.load() || stopToken.stop_requested() || !mRunning.load())
                continue;

            try {
                TheoraReader* reader = activeReader();
                if (!reader || !reader->readFrame(mYCbCr)) {
                    if (mLoop.load() && reader) {
                        reader->seekToTime(getLoopStart());
                        {
                            std::lock_guard tlock(mTimeMutex);
                            mTime = getLoopStart();
                            mLastSeekWallTime = std::chrono::steady_clock::now();
                            mLastSeekSourcePos = getLoopStart();
                        }
                        continue;
                    }
                    mPlaying.store(false);
                    mRunning.store(false);
                    break;
                }
            } catch (const OggEOF&) {
                TheoraReader* reader = activeReader();
                if (mLoop.load() && reader) {
                    reader->seekToTime(getLoopStart());
                    {
                        std::lock_guard tlock(mTimeMutex);
                        mTime = getLoopStart();
                        mLastSeekWallTime = std::chrono::steady_clock::now();
                        mLastSeekSourcePos = getLoopStart();
                    }
                    continue;
                }
                mPlaying.store(false);
                mRunning.store(false);
                break;
            } catch (const OggException& e) {
                std::cerr << "[theora_clip] Error: " << e.what() << std::endl;
                mPlaying.store(false);
                mRunning.store(false);
                break;
            }

            {
                TheoraReader* reader = activeReader();
                std::lock_guard tlock(mTimeMutex);
                if (reader) mTime = reader->getLastDecodedTime();
            }

            mFrameReady.store(true);
            // Défensif : réveille un éventuel consumer en wait_for. Le consumer du
            // modèle latest-wins ne wait pas, mais on garde le notify pour les
            // futurs paths qui pourraient y être ajoutés.
            mFrameCond.notify_all();
        }
        // Producer ne wait PAS pour le consumer. Loop directement vers la prochaine
        // tranche de pacing.
    }
}

void TheoraClip::frameUpdate(float /*dt*/) {
    // Lot AV.5 round 3 — modèle latest-wins : on prend la frame courante quand
    // mFrameReady=true. Pas de mFrameTimer, pas de while-loop multi-blit. Le
    // producer paced (cf. run()) garantit que mYCbCr est rafraîchi à
    // target_speed × native_fps : speed up vient du producer qui change le
    // contenu de mYCbCr plus vite, pas du consumer qui blit plus de frames.
    //
    // Lot AV.5 round 4 — mTime n'est PLUS accumulé ici. Source de vérité = run().
    // Si on accumulait `dt * speed` ici, mTime grandissait N× trop vite à cause
    // des appels multiples par render via Animator BFS, ce qui faussait le
    // mirror seek de setReverse.
    //
    // dt est ignoré (le producer gère son propre clock).

    if (mFrameReady.load()) {
        std::lock_guard lock(mFrameMutex);
        if (mFrameReady.load() && mPlaying.load()) {
            mBlitter->blit(mYCbCr);
            mFramesRendered++;
            {
                std::lock_guard tlock(mTimeMutex);
                mLastBlittedTime = mTime;
                // Round 29 — mLastBlittedFwdFrameIndex est la source de vérité
                // pour le mirror setReverse (frame-index based). Tracker en
                // coord forward, quel que soit le reader actif.
                int blittedFwdIdx = -1;
                if (mReverseActive.load() && mReverseReader) {
                    int revIdx = mReverseReader->getCurrentFrameIndex();
                    int totalFwd = mReader ? mReader->getTotalFrameCount() : 0;
                    blittedFwdIdx = (revIdx >= 0 && totalFwd > 0) ? (totalFwd - 1 - revIdx) : -1;
                } else if (mReader) {
                    blittedFwdIdx = mReader->getCurrentFrameIndex();
                }
                mLastBlittedFwdFrameIndex = blittedFwdIdx;
                // Lot AV.5 round 30 — push (idx, timestamp) dans le ring buffer
                // pour compensation d'input latency dans setReverse(). À chaque
                // blit consumer, on enregistre l'index forward visible AT THAT
                // MOMENT + le timestamp wall-clock. Le setReverse() lookup une
                // entrée "N ms en arrière" (mode wall-time) ou "N ticks en
                // arrière" (mode ticks) pour compenser le délai entre press
                // utilisateur et call setReverse (chain SDL → GamepadNode →
                // JoystickRouter → BFS → clip.update).
                mBlittedFwdRingBuffer[mBlittedFwdRingHead] = blittedFwdIdx;
                mBlittedFwdRingTime[mBlittedFwdRingHead]   = std::chrono::steady_clock::now();
                mBlittedFwdRingHead = (mBlittedFwdRingHead + 1) % RING_SIZE;
            }
            // Round 29 — clear swap-pending and wake the producer thread.
            // Cleared HERE only on a successful blit, so the producer is
            // guaranteed to advance ONLY after the consumer has displayed
            // the mirror frame.
            if (mSwapPending.load()) {
                mSwapPending.store(false);
                mPostSwapBlitCount.fetch_add(1);
                mFrameCond.notify_all();
            }
            // Lot AV.5 round 33 — mFrameReady.store(false) MUST be inside the
            // `(mFrameReady && mPlaying)` block, NOT outside. Sinon :
            //
            // Scenario `pause + setReverse + unpause` (le bug rapporté par
            // l'user). En pause :
            //   - setReverse() pré-décode mirror dans mYCbCr, set mFrameReady=true,
            //     mSwapPending=true. Mais wasPlaying=false → ne rappelle pas play().
            //   - frameUpdate (même tick BFS) entre dans le outer if (mFrameReady).
            //     mPlaying=false → skip blit + skip clear mSwapPending (round 29).
            //   - Si on faisait `mFrameReady.store(false)` ICI (hors du bloc),
            //     mFrameReady passerait à false sans blit → on perd la mirror frame.
            //   - Au prochain unpause, play() restart le thread. mSwapPending est
            //     toujours true (round 32 fix le préserve via condition), mais
            //     mFrameReady = false → consumer ne peut pas blitter la mirror.
            //   - Producer thread voit mSwapPending=true → wait forever. DEADLOCK.
            //
            // Fix : ne clearer mFrameReady QUE si on a effectivement blitté.
            // En pause, on préserve mFrameReady=true et mSwapPending=true tant
            // que le consumer n'a pas réussi à blitter.
            mFrameReady.store(false);
        }
        // else : (!mPlaying) → no clear. Préserver mFrameReady et mSwapPending
        // pour le prochain frameUpdate quand mPlaying redeviendra true.
    }
}

void TheoraClip::setTime(float t) {
    // Signal decode thread to stop touching mReader
    mSeeking.store(true);
    mFrameCond.notify_all(); // unblock if waiting in cond

    // Acquire mFrameMutex — guaranteed safe once thread exits readFrame
    {
        std::lock_guard lock(mFrameMutex);
        mFrameReady.store(false);
        if (auto* r = activeReader()) r->seekToTime(t);
    }
    {
        std::lock_guard lock(mTimeMutex);
        mTime = t;
        mLastBlittedTime = t;
        mFrameTimer = 0.0f;
        mLastSeekWallTime = std::chrono::steady_clock::now();
        mLastSeekSourcePos = t;
    }

    // Resume decode thread
    mSeeking.store(false);
    if (mPlaying.load()) {
        mFrameCond.notify_all();
    }
}

// ── Lot AV.4 round 4 — repro 2006 ReversableClip (I-2056) ─────────────────────
//
// loadReverseStream : ouvre un second TheoraReader sur un fichier pré-rendu inversé
// (généré offline via `ffmpeg -i clip.ogg -vf reverse -af areverse clip_reverse.ogg`,
// ou n'importe quel encodage Theora qui démarre par le dernier frame du clip forward).
// Doit avoir EXACTEMENT les mêmes dimensions + framerate que le forward (sinon le
// blitter va sortir des trucs déformés au swap).
//
// setReverse(bool) : swap atomique du pointeur `mActiveReader`. Le thread de décodage
// utilise ce pointeur à chaque itération de `run()`, donc le swap est instantané
// (à un readFrame près). Reproduction directe de `ReversableClip::doReverse()` 2006.
// Si pas de reverse stream chargé, le swap n'a aucun effet (mActiveReader reste
// sur ForwardReader).
void TheoraClip::loadReverseStream(const std::string& reverseFilename) {
    if (reverseFilename.empty()) return;
    try {
        auto rev = std::make_unique<TheoraReader>(reverseFilename);
        rev->readHeaders();
        // Vérification de cohérence : reverse doit matcher forward (sinon visual mess).
        if (rev->getWidth() != mReader->getWidth()
         || rev->getHeight() != mReader->getHeight()) {
            std::cerr << "[theora_clip] loadReverseStream rejected: dimensions mismatch "
                      << " forward=" << mReader->getWidth() << "x" << mReader->getHeight()
                      << " reverse=" << rev->getWidth() << "x" << rev->getHeight() << std::endl;
            return;
        }
        mReverseReader = std::move(rev);
        std::cout << "[theora_clip] Loaded reverse stream: " << reverseFilename << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "[theora_clip] loadReverseStream failed: " << e.what() << std::endl;
    }
}

void TheoraClip::setReverse(bool reverse) {
    if (reverse == mReverseActive.load()) return;       // no-op
    if (reverse && !mReverseReader) {
        std::cerr << "[theora_clip] setReverse(true) ignored: no reverse stream loaded" << std::endl;
        return;
    }

    // Lot AV.5 round 29 (I-2056) — refonte frame-index based + pre-decode +
    // mSwapPending producer-wait. Combine les insights des rounds précédents :
    //
    //   - Round 3   : producer-paced (latest-wins) ne bloque pas sur consumer.
    //   - Round 5   : mLastBlittedTime tracke la frame VISIBLE (pas la décodée).
    //   - Round 7   : pre-decode mirror frame + mSwapPending pour que le 1ᵉʳ
    //                 blit consumer voit EXACTEMENT la mirror, pas une frame
    //                 avancée par le producer pendant le wakeup.
    //   - Round 28  : asset bombe_reverse.ogg avec stamp anti-dup → forward et
    //                 reverse ont strictement le même nombre de frames (2881).
    //
    // Avec un nb de frames identique, on peut utiliser le FRAME INDEX comme
    // source de vérité (et non le TIME float qui dérive de ε à chaque seek).
    // mirror : forward frame F  ↔  reverse frame (totalFrames - 1 - F).
    //
    // Le POC `poc_reverse_mirror_builder.lua` mode "live" mesure objectivement
    // qu'avec time-based + sans pre-decode, le drift atteint 7-11 frames sur
    // 6 swaps successifs en lecture active. Le nouveau code doit ramener ça
    // à ≤ 2 frames (= tolérance consumer-lag + seek libtheora).

    const bool wasPlaying = mPlaying.load();

    // 1. Stop le producer thread proprement.
    mPlaying.store(false);
    mRunning.store(false);
    mFrameCond.notify_all();
    if (mThread.joinable()) {
        mThread.request_stop();
        mThread.join();
    }
    mPostSwapBlitCount.store(0);

    // 2. Snapshot de la frame VISIBLE (last blitted) en coord forward.
    //
    // Lot AV.5 round 30 — INPUT LATENCY COMPENSATION via ring buffer.
    //
    // Le `mLastBlittedFwdFrameIndex` reflète la dernière frame BLITTÉE par le
    // consumer. Mais entre le moment où l'user appuie sur le bouton gamepad
    // (= ce qu'il voit visuellement) et le moment où `setReverse()` est appelée
    // (via la chaîne SDL → GamepadManager → GamepadNode → JoystickRouterNode →
    // clip.update edge-detect), il s'écoule 1-3 ticks BFS pendant lesquels le
    // clip continue à décoder/blit. Donc `mLastBlittedFwdFrameIndex` AT setReverse
    // ≠ frame que l'user voyait à l'instant du press.
    //
    // Direction du décalage perceptible :
    //   - Forward → reverse swap : pendant la latence, fwd_idx visible avance
    //     de K en forward. Au swap, on mirror la frame "actuelle" (= K en
    //     avant de ce que l'user voyait). Le mirror frame est K frames "dans
    //     le futur" de l'attente. Visuellement = "continuation forward briève
    //     avant inversion" → tolérable, presque imperceptible.
    //   - Reverse → forward swap : pendant la latence, fwd_idx visible descend
    //     de K en reverse. Au swap, mirror = K frames "dans le passé" de
    //     l'attente. Visuellement = "saut en arrière puis reprise forward"
    //     → DÉCALAGE GÊNANT que l'user remarque immédiatement.
    //
    // Fix : utiliser la valeur du ring buffer N entries en arrière (= N ticks
    // BFS plus tôt = la frame VISIBLE au moment du press utilisateur). Default
    // N = 2 (~66ms à 30fps BFS). Tunable via env var BBFX_LATENCY_FRAMES.
    int currentFwdIdx;
    int compLookupAge = -1;  // pour log
    {
        std::lock_guard tlock(mTimeMutex);
        int compensated = -1;
        if (mLatencyMode == LatencyMode::Ms) {
            // Mode wall-time : cherche l'entrée la plus récente avec timestamp
            // <= (now - latency_ms). Robuste aux variations de BFS rate.
            auto cutoff = std::chrono::steady_clock::now()
                        - std::chrono::milliseconds(mLatencyCompensationMs);
            // Parcourt le ring de la plus récente vers la plus ancienne.
            for (int i = 0; i < RING_SIZE; ++i) {
                int probe = ((mBlittedFwdRingHead - 1 - i) % RING_SIZE + RING_SIZE) % RING_SIZE;
                if (mBlittedFwdRingBuffer[probe] < 0) break;  // unwritten
                if (mBlittedFwdRingTime[probe] <= cutoff) {
                    compensated = mBlittedFwdRingBuffer[probe];
                    compLookupAge = i;
                    break;
                }
            }
            // Si TOUS les entries sont plus récents que cutoff, le ring n'a pas
            // assez d'historique → prendre l'entrée la plus ancienne valide.
            if (compensated < 0) {
                for (int i = RING_SIZE - 1; i >= 0; --i) {
                    int probe = ((mBlittedFwdRingHead - 1 - i) % RING_SIZE + RING_SIZE) % RING_SIZE;
                    if (mBlittedFwdRingBuffer[probe] >= 0) {
                        compensated = mBlittedFwdRingBuffer[probe];
                        compLookupAge = i;
                        break;
                    }
                }
            }
        } else {
            // Mode tick-based (legacy round 20) : lookup N entries back.
            int comp = mLatencyCompensationFrames;
            if (comp < 0) comp = 0;
            if (comp >= RING_SIZE) comp = RING_SIZE - 1;
            int lookup = ((mBlittedFwdRingHead - 1 - comp) % RING_SIZE + RING_SIZE) % RING_SIZE;
            compensated = mBlittedFwdRingBuffer[lookup];
            compLookupAge = comp;
        }
        // Fallback : si rien dans le ring (cold start), utilise la valeur courante.
        currentFwdIdx = (compensated >= 0) ? compensated : mLastBlittedFwdFrameIndex;
    }

    // 3. Swap reader (atomic).
    mReverseActive.store(reverse);
    mFrameReady.store(false);
    mSwapPending.store(false);  // sera réarmé en bas après le pre-decode + play().

    TheoraReader* newR = activeReader();
    if (newR) {
        // 4. Calcul du frame-index mirror dans le nouveau stream.
        // Le total est celui du forward (référence). Le reverse stream est
        // garanti même size après round 28 (anti-dup stamp).
        int totalFwd = mReader ? mReader->getTotalFrameCount() : 0;
        int targetIdx;
        if (currentFwdIdx < 0) {
            // Pas encore de frame blittée (clip frais) → seek à 0 du nouveau stream.
            targetIdx = 0;
        } else if (reverse) {
            // forward fwd_idx F ↔ reverse rev_idx (totalFwd - 1 - F).
            targetIdx = totalFwd - 1 - currentFwdIdx;
            if (targetIdx < 0) targetIdx = 0;
            int totalRev = newR->getTotalFrameCount();
            if (totalRev > 0 && targetIdx >= totalRev) targetIdx = totalRev - 1;
        } else {
            // Retour forward : currentFwdIdx EST déjà l'index forward.
            targetIdx = currentFwdIdx;
            if (targetIdx < 0) targetIdx = 0;
            int totalF = newR->getTotalFrameCount();
            if (totalF > 0 && targetIdx >= totalF) targetIdx = totalF - 1;
        }

        // 5. Seek exact via frame index (élimine ε de seekToTime).
        newR->seekToFrameIndex(targetIdx);

        // 6. Pré-décode la mirror frame DANS mYCbCr partagé. Le consumer's
        //    1ᵉʳ blit post-swap verra exactement cette frame, pas la prochaine
        //    que le producer décoderait au wake-up.
        // C4 (mitigation) — écriture sous mFrameMutex : le consumer (getFrame/blit)
        // lit mYCbCr sous ce même mutex ; sans lock ici l'écriture pré-décode peut
        // racer un blit concurrent (frame déchirée). Le producer est join() mais le
        // consumer peut être sur un autre thread que celui appelant setReverse.
        bool preDecoded;
        {
            std::lock_guard lock(mFrameMutex);
            preDecoded = newR->getCurrentFrame(mYCbCr);
        }

        // 7. Sync mTime / mLastBlittedTime / mLastBlittedFwdFrameIndex sur la
        //    landed frame, pour que les futurs setReverse() partent juste.
        float landed_t = 0.0f;
        if (newR->getCurrentFrameIndex() == targetIdx) {
            landed_t = static_cast<float>(targetIdx) / newR->getFrameRate();
        }
        {
            std::lock_guard tlock(mTimeMutex);
            mTime                       = landed_t;
            mLastBlittedTime            = landed_t;
            // En coord forward : si on est en reverse, totalFwd-1-targetIdx,
            // sinon targetIdx.
            mLastBlittedFwdFrameIndex   = reverse ? (totalFwd - 1 - targetIdx)
                                                 : targetIdx;
            mLastSeekWallTime           = std::chrono::steady_clock::now();
            mLastSeekSourcePos          = landed_t;
        }

        if (mBlitter) mBlitter->resetDiagnostics();

        std::cout << "[theora_clip] setReverse(" << (reverse ? "true" : "false")
                  << "): fwd_pre=" << currentFwdIdx
                  << " (lat_age=" << compLookupAge
                  << (mLatencyMode == LatencyMode::Ms ? " ms-mode"  : " tick-mode") << ")"
                  << " targetIdx=" << targetIdx
                  << " landed_t=" << landed_t
                  << " pre_decoded=" << (preDecoded ? 1 : 0)
                  << std::endl;
    }

    // 8. Arme mSwapPending AVANT play() pour que le producer thread, dès son
    //    1er iter post-start, voie mSwapPending=true et attende le consumer.
    //    (Race sinon : producer décode + écrase mYCbCr avant qu'on ait le temps
    //     de set mSwapPending.)
    mSwapPending.store(true);

    // 9. Restart producer thread. Note : play() (fresh-start path) reset
    //    mFrameReady=false, c'est pourquoi on set mFrameReady=true APRÈS.
    if (wasPlaying) play();

    // 10. Arme le 1ᵉʳ blit : mFrameReady=true → consumer blit la mYCbCr
    //     pré-decoded. Après blit, frameUpdate() clear mSwapPending → notify
    //     → producer wake up et continue son pacing normal.
    mFrameReady.store(true);
    mFrameCond.notify_all();
}

} // namespace bbfx
