#include "SpectrogramTextureNode.h"
#include "../core/Animator.h"
#include "../audio/AudioCapture.h"
#include <OgreTextureManager.h>
#include <OgreHardwarePixelBuffer.h>
#include <algorithm>
#include <cmath>
#include <cstring>

namespace bbfx {

// N9 — vraie FFT (radix-2 itérative, fenêtre de Hann) sur les échantillons audio.
// Remplit `mags` avec la demi-bande de magnitudes (fréquence linéaire 0..Nyquist).
// Avant, le spectrogramme moyennait l'amplitude temporelle par tranches → ce n'était
// pas un spectre fréquentiel.
static void computeFftMagnitudes(const std::vector<float>& samples, std::vector<float>& mags) {
    int N = 1;
    while (N * 2 <= static_cast<int>(samples.size()) && N < 4096) N *= 2;
    if (N < 2) { mags.clear(); return; }
    std::vector<float> re(N), im(N, 0.0f);
    const float PI = 3.14159265358979323846f;
    for (int i = 0; i < N; ++i) {
        float w = 0.5f * (1.0f - std::cos(2.0f * PI * i / (N - 1))); // Hann
        re[i] = samples[i] * w;
    }
    // bit-reversal
    for (int i = 1, j = 0; i < N; ++i) {
        int bit = N >> 1;
        for (; j & bit; bit >>= 1) j ^= bit;
        j ^= bit;
        if (i < j) { std::swap(re[i], re[j]); std::swap(im[i], im[j]); }
    }
    for (int len = 2; len <= N; len <<= 1) {
        float ang = -2.0f * PI / len;
        float wlenRe = std::cos(ang), wlenIm = std::sin(ang);
        for (int i = 0; i < N; i += len) {
            float wRe = 1.0f, wIm = 0.0f;
            for (int k = 0; k < len / 2; ++k) {
                float uRe = re[i + k],         uIm = im[i + k];
                float vRe = re[i + k + len/2] * wRe - im[i + k + len/2] * wIm;
                float vIm = re[i + k + len/2] * wIm + im[i + k + len/2] * wRe;
                re[i + k]         = uRe + vRe; im[i + k]         = uIm + vIm;
                re[i + k + len/2] = uRe - vRe; im[i + k + len/2] = uIm - vIm;
                float nwRe = wRe * wlenRe - wIm * wlenIm;
                wIm = wRe * wlenIm + wIm * wlenRe; wRe = nwRe;
            }
        }
    }
    int half = N / 2;
    mags.resize(half);
    float norm = 2.0f / N;
    for (int i = 0; i < half; ++i)
        mags[i] = std::sqrt(re[i] * re[i] + im[i] * im[i]) * norm;
}

int SpectrogramTextureNode::sCounter = 0;

SpectrogramTextureNode::SpectrogramTextureNode(const std::string& name)
    : AnimationNode(name)
{
    sCounter++;
    mTexName = "Spectrogram/" + std::to_string(sCounter) + "/" + getName();

    ParamDef wDef;
    wDef.name = "width"; wDef.label = "Width (frequency)";
    wDef.type = ParamType::INT;
    wDef.intVal = 256; wDef.minVal = 64; wDef.maxVal = 1024;
    mSpec.addParam(wDef);

    ParamDef hDef;
    hDef.name = "height"; hDef.label = "Height (history)";
    hDef.type = ParamType::INT;
    hDef.intVal = 128; hDef.minVal = 64; hDef.maxVal = 1024;
    mSpec.addParam(hDef);

    ParamDef fDef;
    fDef.name = "frequency_scale"; fDef.label = "Frequency Scale";
    fDef.type = ParamType::ENUM;
    fDef.stringVal = "log";
    fDef.choices = {"linear", "log", "mel"};
    mSpec.addParam(fDef);

    ParamDef iDef;
    iDef.name = "intensity_scale"; iDef.label = "Intensity Scale";
    iDef.type = ParamType::ENUM;
    iDef.stringVal = "db";
    iDef.choices = {"linear", "log", "db"};
    mSpec.addParam(iDef);

    ParamDef cDef;
    cDef.name = "colormap"; cDef.label = "Colormap";
    cDef.type = ParamType::ENUM;
    cDef.stringVal = "grayscale";
    cDef.choices = {"grayscale", "viridis", "plasma", "magma"};
    mSpec.addParam(cDef);

    // v3.5.2 cleanup post-Sprint S5: expose the RTT name so MaterialBridgeNode
    // (Lot T) can pull it dynamically via `material_source` port. Pattern aligned
    // with GrayscaleNode (Lot U) and NoiseTextureNode.
    ParamDef outDef;
    outDef.name = "texture_out"; outDef.label = "Texture (out)";
    outDef.type = ParamType::STRING;
    outDef.stringVal = mTexName;
    outDef.readOnly = true;
    outDef.tooltip = "Read-only: name of the spectrogram waterfall RTT. Routable to MaterialBridgeNode.";
    mSpec.addParam(outDef);

    setParamSpec(&mSpec);

    addInput(new AnimationPort("audio",       0.0f, true)); // N9 — entity-link (marqueur) vers AudioCaptureNode
    addInput(new AnimationPort("time_offset", 0.0f));

    // v3.5.2 Sprint S7 Lot AA — texture_ready output (border pulse green when active).
    addOutput(new AnimationPort("texture_ready", 0.0f));

    // v3.5.2 Sprint S7 Lot Z — port tooltips
    setPortTooltip("audio",         "Entity-link to an AudioCaptureNode. Pulls FFT magnitudes for the spectrogram column.");
    setPortTooltip("time_offset",   "Adds to the time axis used for the scrolling spectrogram (animate for parallax).");
    setPortTooltip("texture_ready", "Pulses 1.0 each frame the spectrogram column was redrawn (always 1.0 when enabled).");
}

SpectrogramTextureNode::~SpectrogramTextureNode() {
    cleanup();
}

void SpectrogramTextureNode::ensureTexture() {
    if (!mTex.isNull() && (int)mTex->getWidth() == mWidth && (int)mTex->getHeight() == mHeight) return;
    if (!mTex.isNull()) {
        Ogre::TextureManager::getSingleton().remove(mTexName);
        mTex.setNull();
    }
    mTex = Ogre::TextureManager::getSingleton().createManual(
        mTexName,
        Ogre::ResourceGroupManager::DEFAULT_RESOURCE_GROUP_NAME,
        Ogre::TEX_TYPE_2D,
        mWidth, mHeight, 0,
        Ogre::PF_BYTE_RGBA,
        Ogre::TU_DYNAMIC_WRITE_ONLY_DISCARDABLE);
    mScrollOffset = 0;
}

void SpectrogramTextureNode::applyColormap(float intensity, Colormap cm,
                                            uint8_t& r, uint8_t& g, uint8_t& b) {
    intensity = std::clamp(intensity, 0.0f, 1.0f);
    uint8_t i = static_cast<uint8_t>(intensity * 255.0f);
    switch (cm) {
    case Colormap::Grayscale:
        r = g = b = i; break;
    case Colormap::Viridis: {
        // Approximate Viridis: dark blue/purple -> teal -> green -> yellow.
        float t = intensity;
        r = static_cast<uint8_t>((0.267f + t * (0.992f - 0.267f)) * 255.0f);
        g = static_cast<uint8_t>((0.005f + t * (0.906f - 0.005f)) * 255.0f);
        b = static_cast<uint8_t>((0.329f + t * (0.144f - 0.329f)) * 255.0f);
        break;
    }
    case Colormap::Plasma: {
        float t = intensity;
        r = static_cast<uint8_t>((0.050f + t * (0.940f - 0.050f)) * 255.0f);
        g = static_cast<uint8_t>((0.030f + t * (0.975f - 0.030f)) * 255.0f);
        b = static_cast<uint8_t>((0.530f + t * (0.131f - 0.530f)) * 255.0f);
        break;
    }
    case Colormap::Magma: {
        float t = intensity;
        r = static_cast<uint8_t>((0.001f + t * (0.987f - 0.001f)) * 255.0f);
        g = static_cast<uint8_t>((0.001f + t * (0.997f - 0.001f)) * 255.0f);
        b = static_cast<uint8_t>((0.013f + t * (0.749f - 0.013f)) * 255.0f);
        break;
    }
    }
}

AnimationNode* SpectrogramTextureNode::resolveAudio() {
    auto* animator = Animator::instance();
    if (!animator) return nullptr;
    auto& in = getInputs();
    auto it = in.find("audio");
    if (it == in.end()) return nullptr;
    auto sources = animator->getSourceNodes(it->second);
    for (auto* s : sources) {
        if (auto* a = dynamic_cast<AudioCaptureNode*>(s)) return a;
    }
    return nullptr;
}

void SpectrogramTextureNode::buildSyntheticBins(int height, std::vector<float>& bins, float timeOffset) {
    bins.resize(height);
    for (int i = 0; i < height; ++i) {
        float freq = (float)i / std::max(1, height - 1);
        // Pseudo-spectrum: low frequencies pulse, mid/high decay with time offset.
        float bass = std::max(0.0f, std::sin(timeOffset * 2.0f + freq * 12.0f) * (1.0f - freq));
        float noise = 0.5f * std::abs(std::sin(timeOffset * 5.7f + freq * 31.7f));
        bins[i] = std::clamp(bass + noise * (0.3f + 0.7f * freq), 0.0f, 1.0f);
    }
}

void SpectrogramTextureNode::writeColumn(int /*colX*/, const std::vector<float>& bins) {
    if (mTex.isNull()) return;

    // v3.5.2 final: ring-buffer pointer + per-instance shadow.
    // Each frame we shift the write pointer (mScrollOffset) and rasterize
    // with a "virtual scroll" — UV.x is offset by mScrollOffset/width when
    // sampling the shadow. The shadow is per-instance (not thread_local)
    // so multiple SpectrogramTextureNodes coexist correctly.
    // Réallouer si la hauteur OU la largeur a changé : ne vérifier que la hauteur
    // laissait les sous-vecteurs à l'ancienne largeur → accès hors-bornes quand
    // mWidth augmente (writeCol / ringCol >= ancienne largeur).
    if ((int)mShadow.size() != mHeight ||
        (mHeight > 0 && (int)mShadow[0].size() != mWidth)) {
        mShadow.assign(mHeight, std::vector<float>(mWidth, 0.0f));
        mScrollOffset = 0;
    }
    if (mWidth <= 0 || mHeight <= 0) return;
    // Write the latest bins at the ring head (no shift — ring pointer advances).
    int writeCol = mScrollOffset % mWidth;
    for (int y = 0; y < mHeight && y < (int)bins.size(); ++y) {
        mShadow[y][writeCol] = bins[y];
    }

    auto buf = mTex->getBuffer();
    buf->lock(Ogre::HardwareBuffer::HBL_DISCARD);
    const auto& pb = buf->getCurrentLock();
    auto* dst = static_cast<uint8_t*>(pb.data);

    // Rasterize: column X of the texture maps to ring slot (writeCol + 1 + X) % width.
    // The newly-written column lands on the rightmost edge.
    int W = mWidth;
    int H = mHeight;
    for (int y = 0; y < H; ++y) {
        // Frequency-scale: remap row index Y to a source row in mShadow.
        float freq01 = (float)y / std::max(1, H - 1);
        if (mFreqScale == FreqScale::Log) {
            freq01 = std::log10(1.0f + 9.0f * freq01);
        } else if (mFreqScale == FreqScale::Mel) {
            freq01 = std::log10(1.0f + 9.0f * freq01) * 0.91f;
        }
        int srcY = std::clamp((int)std::round(freq01 * (H - 1)), 0, H - 1);

        uint8_t* row = dst + y * pb.rowPitch * 4;
        const auto& shadowRow = mShadow[srcY];
        for (int x = 0; x < W; ++x) {
            // Map output X to ring slot: oldest is just-after writeCol, newest is writeCol.
            int ringIdx = (writeCol + 1 + x) % W;
            float v = shadowRow[ringIdx];
            if (mIntensityScale == IntensityScale::Log) {
                v = std::log10(1.0f + 9.0f * std::clamp(v, 0.0f, 1.0f));
            } else if (mIntensityScale == IntensityScale::Db) {
                v = std::clamp(v, 1e-4f, 1.0f);
                v = std::clamp((20.0f * std::log10(v) + 60.0f) / 60.0f, 0.0f, 1.0f);
            }
            uint8_t r, g, b;
            applyColormap(v, mColormap, r, g, b);
            row[x * 4 + 0] = r;
            row[x * 4 + 1] = g;
            row[x * 4 + 2] = b;
            row[x * 4 + 3] = 255;
        }
    }
    buf->unlock();

    // Advance ring pointer
    mScrollOffset = (mScrollOffset + 1) % std::max(1, W);
}

void SpectrogramTextureNode::update() {
    if (!mEnabled) return;

    auto* wParam = mSpec.getParam("width");
    auto* hParam = mSpec.getParam("height");
    if (wParam) mWidth  = std::clamp(wParam->intVal, 64, 1024);
    if (hParam) mHeight = std::clamp(hParam->intVal, 64, 1024);

    auto* fParam = mSpec.getParam("frequency_scale");
    if (fParam) {
        if      (fParam->stringVal == "linear") mFreqScale = FreqScale::Linear;
        else if (fParam->stringVal == "mel")    mFreqScale = FreqScale::Mel;
        else                                     mFreqScale = FreqScale::Log;
    }
    auto* iParam = mSpec.getParam("intensity_scale");
    if (iParam) {
        if      (iParam->stringVal == "linear") mIntensityScale = IntensityScale::Linear;
        else if (iParam->stringVal == "log")    mIntensityScale = IntensityScale::Log;
        else                                     mIntensityScale = IntensityScale::Db;
    }
    auto* cParam = mSpec.getParam("colormap");
    if (cParam) {
        if      (cParam->stringVal == "viridis") mColormap = Colormap::Viridis;
        else if (cParam->stringVal == "plasma")  mColormap = Colormap::Plasma;
        else if (cParam->stringVal == "magma")   mColormap = Colormap::Magma;
        else                                      mColormap = Colormap::Grayscale;
    }

    ensureTexture();

    // Build the new bin column: from audio source if present, else synthetic.
    std::vector<float> bins;
    auto* audio = dynamic_cast<AudioCaptureNode*>(resolveAudio());
    if (audio) {
        const auto& samples = audio->getSamples();
        // N9 — vraie magnitude FFT (spectre fréquentiel) ré-échantillonnée sur mHeight
        // lignes (ligne basse = basse fréquence). writeColumn applique ensuite le
        // remap d'échelle (log/mel) à l'affichage.
        bins.resize(mHeight, 0.0f);
        if (!samples.empty()) {
            std::vector<float> mags;
            computeFftMagnitudes(samples, mags);
            int specN = static_cast<int>(mags.size());
            if (specN > 0) {
                for (int i = 0; i < mHeight; ++i) {
                    int b0 = std::clamp(i * specN / mHeight, 0, specN - 1);
                    int b1 = std::clamp((i + 1) * specN / mHeight, b0 + 1, specN);
                    float acc = 0.0f;
                    for (int b = b0; b < b1; ++b) acc += mags[b];
                    // gain ×4 pour rendre les magnitudes visibles (clamp [0,1]).
                    bins[i] = std::clamp(acc / (b1 - b0) * 4.0f, 0.0f, 1.0f);
                }
            }
        }
    } else {
        float to = 0.0f;
        auto& in = getInputs();
        auto it = in.find("time_offset");
        if (it != in.end()) to = it->second->getValue();
        buildSyntheticBins(mHeight, bins, to);
    }

    writeColumn(mWidth - 1, bins);

    // Refresh `texture_out` mirror (consumed by MaterialBridgeNode auto-wrap).
    if (auto* outMir = mSpec.getParam("texture_out")) outMir->stringVal = mTexName;

    // v3.5.2 Sprint S7 Lot AA — pulse texture_ready 1.0 (column written each update).
    auto& outs = getOutputs();
    if (auto it = outs.find("texture_ready"); it != outs.end()) {
        it->second->setValue(1.0f);
    }

    fireUpdate();
}

void SpectrogramTextureNode::cleanup() {
    if (!mTex.isNull()) {
        Ogre::TextureManager::getSingleton().remove(mTexName);
        mTex.setNull();
    }
}

} // namespace bbfx
