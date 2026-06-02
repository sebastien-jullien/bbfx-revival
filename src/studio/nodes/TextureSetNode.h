#pragma once

#include "../../core/AnimationNode.h"
#include "../../core/ParamSpec.h"
#include <string>
#include <vector>

namespace bbfx {

/// TextureSetNode — repro 2006 fidèle (cf. textureset.lua:120-258).
///
/// Un « TextureSet » avance un COUPLE de textures `{gray, color, factor}`
/// synchronisé. Sur un `next` (trigger edge-detect), les deux couches changent
/// ENSEMBLE de texture et le `factor` (multiplicateur de vitesse pour layer B,
/// typiquement négatif pour le sens opposé) est exposé en sortie.
///
/// Format du paramètre `presets` (STRING) :
///   "gray1|color1|factor1;gray2|color2|factor2;..."
/// Exemple : "BumpyMetal.jpg|RustySteel.jpg|-0.7;Chrome.jpg|Water01.jpg|-0.5"
///
/// Sorties consommables :
///   - texture_a_ready / texture_b_ready : FLOAT 1.0 pendant transition active
///     (compat avec les autres nodes qui consomment ces ports comme entity-link)
///   - factor : FLOAT, multiplicateur courant — à linker sur TextureBlendNode.factor
///   - current_index : FLOAT (index entier du couple courant)
///   - transition_progress : FLOAT 0..1
///
/// Mirrors ParamSpec readonly (pour Pattern 3 entity-link consumer) :
///   - current_texture_a : nom de la texture A (= gray) courante
///   - current_texture_b : nom de la texture B (= color) courante
///   - current_factor   : valeur courante du factor
class TextureSetNode : public AnimationNode {
public:
    struct Preset { std::string gray; std::string color; float factor; };

    explicit TextureSetNode(const std::string& name);
    ~TextureSetNode() override = default;

    void update() override;
    std::string getTypeName() const override { return "TextureSetNode"; }

    // Accesseurs pour les tests (TSET-001..006)
    void setPresets(const std::vector<Preset>& list);
    const std::vector<Preset>& getPresets() const { return mPresets; }
    int  getCurrentIndex() const { return mCurrentIndex; }
    int  getNextIndex()    const { return mNextIndex; }
    float getCurrentFactor() const;
    const std::string& getCurrentTextureA() const;
    const std::string& getCurrentTextureB() const;
    float getTransitionProgress() const { return mTransitionProgress; }

    // Triggers publics (bypass DAG, utilisés par les tests).
    void triggerNext();
    void triggerPrev();
    void triggerGoto(int idx);

private:
    ParamSpec mSpec;
    std::vector<Preset> mPresets;
    int  mCurrentIndex = 0;
    int  mNextIndex = 0;
    float mTransitionProgress = 0.0f;
    float mTransitionTime = 1.0f;

    // Edge detect
    bool mPrevNextState = false;
    bool mPrevPrevState = false;
    int  mPrevGotoIndex = -1;
    float mPrevDt = 0.0f;

    // Cached canonical form of `presets` to detect ParamSpec edits
    std::string mPrevPresetsStr;

    void parsePresetsFromParam();
    void commitPendingTransition();
};

} // namespace bbfx
