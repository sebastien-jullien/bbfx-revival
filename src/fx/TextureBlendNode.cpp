#include "TextureBlendNode.h"
#include "../core/Animator.h"
#include "../core/DebugLog.h"
#include <OgreMaterialManager.h>
#include <OgreTechnique.h>
#include <OgrePass.h>
#include <OgreTextureUnitState.h>
#include <OgreRTShaderSystem.h>
#include <algorithm>
#include <iostream>

namespace bbfx {

int TextureBlendNode::sCounter = 0;

TextureBlendNode::TextureBlendNode(const std::string& name)
    : AnimationNode(name)
{
    sCounter++;

    // ParamSpec — texture inputs + blend mode + mask invert
    ParamDef texADef;
    texADef.name = "tex_a"; texADef.label = "Texture A";
    texADef.type = ParamType::TEXTURE; texADef.stringVal = "void.png";
    mSpec.addParam(texADef);

    ParamDef texBDef;
    texBDef.name = "tex_b"; texBDef.label = "Texture B";
    texBDef.type = ParamType::TEXTURE; texBDef.stringVal = "void.png";
    mSpec.addParam(texBDef);

    ParamDef maskDef;
    maskDef.name = "mask"; maskDef.label = "Mask";
    maskDef.type = ParamType::TEXTURE; maskDef.stringVal = "gradient.png";
    mSpec.addParam(maskDef);

    ParamDef bmDef;
    bmDef.name = "blend_mode"; bmDef.label = "Blend Mode";
    bmDef.type = ParamType::ENUM;
    bmDef.stringVal = "alpha";
    bmDef.choices = {"alpha", "add", "multiply", "screen"};
    mSpec.addParam(bmDef);

    ParamDef miDef;
    miDef.name = "mask_invert"; miDef.label = "Mask Invert";
    miDef.type = ParamType::BOOL; miDef.boolVal = false;
    mSpec.addParam(miDef);

    // Read-only mirror for the generated material name.
    ParamDef matOutDef;
    matOutDef.name = "material_out"; matOutDef.label = "Material (out)";
    matOutDef.type = ParamType::STRING; matOutDef.stringVal = "";
    matOutDef.readOnly = true;
    matOutDef.tooltip = "Read-only: live material name produced by this blend. Routable to MaterialBridgeNode.";
    mSpec.addParam(matOutDef);

    setParamSpec(&mSpec);

    // Animation ports — Layer A
    addInput(new AnimationPort("scroll_u_a", 0.0f));
    addInput(new AnimationPort("scroll_v_a", 0.0f));
    addInput(new AnimationPort("rotate_a",   0.0f));
    addInput(new AnimationPort("tex_scale_u_a", 1.0f));
    addInput(new AnimationPort("tex_scale_v_a", 1.0f));

    // Animation ports — Layer B
    addInput(new AnimationPort("scroll_u_b", 0.0f));
    addInput(new AnimationPort("scroll_v_b", 0.0f));
    addInput(new AnimationPort("rotate_b",   0.0f));
    addInput(new AnimationPort("tex_scale_u_b", 1.0f));
    addInput(new AnimationPort("tex_scale_v_b", 1.0f));

    // Mask animation — U and V offsets of the mask layer (the "sweep" of the reveal).
    addInput(new AnimationPort("mask_offset_u", 0.0f));
    addInput(new AnimationPort("mask_offset_v", 0.0f));

    // Dynamic texture sources (Pattern 3) — link a producer (TextureCycleNode, GrayscaleNode,
    // SpectrogramTextureNode…) to drive layer A / layer B textures from the DAG instead of the
    // static tex_a/tex_b params. Pulls the upstream's current_texture / texture_out / texture mirror.
    addInput(new AnimationPort("tex_a_source", 0.0f, true));   // entity-link
    addInput(new AnimationPort("tex_b_source", 0.0f, true));   // entity-link

    // Lot AU.23 — Time-integrated SPEED ports (= the 2006 `TimePulseController`).
    // Each frame: accumulator += dt * speed, then the TUS scroll/rotate/mask-offset is
    // (accumulator + corresponding static port). Lets layer A and layer B scroll continuously,
    // at OPPOSITE signs (the 2006 "two textures crossing through the mask" effect — `u_a=+s`,
    // `u_b=-s*factor`). `dt` defaults to 1/60 if not linked; `time.dt → blend.dt` is the canonical wiring.
    addInput(new AnimationPort("dt",                  1.0f / 60.0f));
    addInput(new AnimationPort("scroll_u_a_speed",    0.0f));
    addInput(new AnimationPort("scroll_v_a_speed",    0.0f));
    addInput(new AnimationPort("scroll_u_b_speed",    0.0f));
    addInput(new AnimationPort("scroll_v_b_speed",    0.0f));
    addInput(new AnimationPort("rotate_a_speed",      0.0f));
    addInput(new AnimationPort("rotate_b_speed",      0.0f));
    addInput(new AnimationPort("mask_offset_u_speed", 0.0f));
    addInput(new AnimationPort("mask_offset_v_speed", 0.0f));

    // Lot AV — Repro 2006 « Texture Set » : `factor` multiplie les vitesses de
    // layer B (cf. textureset.lua:249-255, factor négatif → sens opposé), et
    // `u_amp` / `v_amp` clampent les vitesses dans [-amp, +amp] (= les
    // setSpeedBounds du SlideControllerValue 2006). À piloter depuis un
    // TextureSetNode (port `factor`) et un JoystickRouterNode (mode scroll_gate)
    // bornés par u_amp/v_amp.
    // Défauts choisis pour rester compatible avec les tests BLD-009 préexistants
    // (speeds ±1.0/±0.5 → pas clampés) : u_amp/v_amp = 10.0 = pas de clamp en
    // pratique. Le builder demo_texture_set rebaisse à 0.2 / 0.1 pour 2006 strict.
    addInput(new AnimationPort("factor", 1.0f));
    addInput(new AnimationPort("u_amp",  10.0f));
    addInput(new AnimationPort("v_amp",  10.0f));

    // Generated material name as a number-coded "presence" (1.0 = ready, 0 = not ready).
    addOutput(new AnimationPort("material_ready", 0.0f));

    mGeneratedMaterialName = "TextureBlend/" + std::to_string(sCounter) + "/" + getName();

    // v3.5.2 Sprint S7 Lot Z — port tooltips
    setPortTooltip("scroll_u_a",     "Layer A horizontal scroll speed (uv units / sec). Animatable.");
    setPortTooltip("scroll_v_a",     "Layer A vertical scroll speed (uv units / sec). Animatable.");
    setPortTooltip("rotate_a",       "Layer A texture rotation (radians). Animatable.");
    setPortTooltip("tex_scale_u_a",  "Layer A horizontal tile scale (1.0 = no tiling).");
    setPortTooltip("tex_scale_v_a",  "Layer A vertical tile scale (1.0 = no tiling).");
    setPortTooltip("scroll_u_b",     "Layer B horizontal scroll speed (uv units / sec). Animatable.");
    setPortTooltip("scroll_v_b",     "Layer B vertical scroll speed (uv units / sec). Animatable.");
    setPortTooltip("rotate_b",       "Layer B texture rotation (radians). Animatable.");
    setPortTooltip("tex_scale_u_b",  "Layer B horizontal tile scale (1.0 = no tiling).");
    setPortTooltip("tex_scale_v_b",  "Layer B vertical tile scale (1.0 = no tiling).");
    setPortTooltip("mask_offset_u",  "Horizontal U-offset of the mask layer (slides the reveal left/right — drive it from a stick).");
    setPortTooltip("mask_offset_v",  "Vertical V-offset of the mask layer (drives the 2006 sweep transition).");
    setPortTooltip("tex_a_source",   "Entity-link: pull layer A's texture name from an upstream producer (TextureCycle current_texture, Grayscale texture_out, …). Overrides the tex_a param when linked.");
    setPortTooltip("tex_b_source",   "Entity-link: pull layer B's texture name from an upstream producer. Overrides the tex_b param when linked.");
    setPortTooltip("dt",                  "Frame delta (s). Linked from time.dt — drives the *_speed integrators below. Default 1/60.");
    setPortTooltip("scroll_u_a_speed",    "Layer A U-scroll SPEED (uv units / sec). Integrated continuously by the node (= the 2006 TimePulseController).");
    setPortTooltip("scroll_v_a_speed",    "Layer A V-scroll SPEED (uv units / sec). Integrated continuously.");
    setPortTooltip("scroll_u_b_speed",    "Layer B U-scroll SPEED (uv units / sec). Set opposite sign vs. layer A for the 2006 'crossing' effect.");
    setPortTooltip("scroll_v_b_speed",    "Layer B V-scroll SPEED (uv units / sec). Integrated continuously.");
    setPortTooltip("rotate_a_speed",      "Layer A rotation SPEED (rad / sec). Integrated continuously.");
    setPortTooltip("rotate_b_speed",      "Layer B rotation SPEED (rad / sec).");
    setPortTooltip("mask_offset_u_speed", "Mask U-offset SPEED. Integrated continuously.");
    setPortTooltip("mask_offset_v_speed", "Mask V-offset SPEED (= the 2006 vsweep). Drives the continuous sweep of the reveal.");
    setPortTooltip("factor", "Lot AV — Multiplicateur appliqué à scroll_u_b_speed / scroll_v_b_speed (= 2006 preset.factor). Négatif = sens opposé.");
    setPortTooltip("u_amp",  "Lot AV — Borne ± de la vitesse U intégrée (clamp scroll_u_a_speed et scroll_u_b_speed×factor dans [-u_amp, +u_amp]).");
    setPortTooltip("v_amp",  "Lot AV — Borne ± de la vitesse V intégrée (idem U).");
    setPortTooltip("material_ready", "Pulses 1.0 when the generated material has all textures bound (out -> MaterialBridge).");
}

void TextureBlendNode::buildMaterialIfNeeded() {
    if (!mGeneratedMaterial.isNull()) return;

    auto& mm = Ogre::MaterialManager::getSingleton();
    // Create the material FRESH in the DEFAULT ("General") resource group — NOT by cloning
    // BaseWhite. BaseWhite lives in OGRE's internal group, which has no resource locations
    // registered: a clone of it inherits that group, so its texture units can't resolve their
    // texture files (RustySteel.jpg, …) and the material renders blank — invisible on a
    // fullscreen overlay, BaseWhite on a mesh. A material created in "General" finds the
    // textures (the resource locations are there) and is fixed-function (proven render path,
    // same as MaterialBridge/FSO auto-wrap).
    mGeneratedMaterial = mm.getByName(mGeneratedMaterialName);
    if (mGeneratedMaterial.isNull()) {
        mGeneratedMaterial = mm.create(mGeneratedMaterialName,
                                       Ogre::ResourceGroupManager::DEFAULT_RESOURCE_GROUP_NAME);
    }

    // Ensure 3 TUS in pass 0.
    auto* pass = mGeneratedMaterial->getTechnique(0)->getPass(0);
    while (pass->getNumTextureUnitStates() < 3) {
        pass->createTextureUnitState();
    }
    // A texture compositor — display the textures directly, not lit-shaded.
    pass->setLightingEnabled(false);

    // TUS layout: 0 = layer A (base), 1 = mask (its alpha = blend factor), 2 = layer B (blended over A).
    pass->getTextureUnitState(0)->setTextureName("void.png");        // tex_a
    pass->getTextureUnitState(1)->setTextureName("gradient.png");    // mask
    pass->getTextureUnitState(2)->setTextureName("void.png");        // tex_b

    auto* matOutMir = mSpec.getParam("material_out");
    if (matOutMir) matOutMir->stringVal = mGeneratedMaterialName;
}

void TextureBlendNode::applyBlendMode() {
    if (mGeneratedMaterial.isNull()) return;
    auto* pass = mGeneratedMaterial->getTechnique(0)->getPass(0);
    if (pass->getNumTextureUnitStates() < 3) return;
    using namespace Ogre;

    // TUS 0 = layer A (color = its texture, alpha = 1). TUS 1 = mask: keep the running color
    // (= layer A), but let its alpha modulate through → after TUS 1 the running alpha = mask alpha.
    auto* tusMask = pass->getTextureUnitState(1);
    tusMask->setColourOperationEx(LBX_SOURCE1, LBS_CURRENT, LBS_CURRENT);   // color unchanged
    tusMask->setAlphaOperation(LBX_SOURCE1, LBS_TEXTURE, LBS_CURRENT);      // running alpha := mask texture alpha

    // TUS 2 = layer B, combined with layer A according to the blend mode (alpha = the mask).
    auto* tusB = pass->getTextureUnitState(2);
    switch (mBlendMode) {
    case BlendMode::Alpha:
        // lerp(A, B, mask) — the historic "fader" / sweep transition.
        tusB->setColourOperationEx(LBX_BLEND_CURRENT_ALPHA, LBS_TEXTURE, LBS_CURRENT);
        break;
    case BlendMode::Add:
        tusB->setColourOperationEx(LBX_ADD, LBS_TEXTURE, LBS_CURRENT);
        break;
    case BlendMode::Multiply:
        tusB->setColourOperationEx(LBX_MODULATE, LBS_TEXTURE, LBS_CURRENT);
        break;
    case BlendMode::Screen:
        tusB->setColourOperationEx(LBX_MODULATE_X2, LBS_TEXTURE, LBS_CURRENT);
        break;
    }
    // The material's OUTPUT alpha is uniform (= layer B's texture alpha, normally 1.0) — the mask
    // only drives the COLOUR blend (lerp A↔B). The overall opacity is controlled by the consumer
    // (e.g. FullscreenOverlayNode's `alpha` port), not by the mask shape — like the 2006 TextureSet.
    tusB->setAlphaOperation(LBX_SOURCE1, LBS_TEXTURE, LBS_CURRENT);
}

void TextureBlendNode::applyTUSTransforms(int tusIndex,
                                          float scrollU, float scrollV,
                                          float rotate, float scaleU, float scaleV,
                                          float& prevU, float& prevV, float& prevR,
                                          float& prevSU, float& prevSV)
{
    if (mGeneratedMaterial.isNull()) return;
    auto* pass = mGeneratedMaterial->getTechnique(0)->getPass(0);
    if (tusIndex < 0 || tusIndex >= static_cast<int>(pass->getNumTextureUnitStates())) return;
    auto* tus = pass->getTextureUnitState(tusIndex);

    if (scrollU != prevU || scrollV != prevV) {
        tus->setTextureScroll(scrollU, scrollV);
        prevU = scrollU; prevV = scrollV;
    }
    if (rotate != prevR) {
        tus->setTextureRotate(Ogre::Radian(rotate));
        prevR = rotate;
    }
    if (scaleU != prevSU || scaleV != prevSV) {
        tus->setTextureScale(scaleU, scaleV);
        prevSU = scaleU; prevSV = scaleV;
    }
}

void TextureBlendNode::update() {
    if (!mEnabled) return;

    buildMaterialIfNeeded();
    if (mGeneratedMaterial.isNull()) return;

    auto* pass = mGeneratedMaterial->getTechnique(0)->getPass(0);

    // Sync texture names. A linked `tex_a_source` / `tex_b_source` entity-link port (Pattern 3)
    // overrides the static tex_a / tex_b param — it pulls the upstream producer's mirror
    // (TextureCycle.current_texture, Grayscale.texture_out, Spectrogram.texture, …).
    auto* texAParam = mSpec.getParam("tex_a");
    auto* texBParam = mSpec.getParam("tex_b");
    auto* maskParam = mSpec.getParam("mask");
    // Lot AV.3 (I-2053 round 3) — la liste des mirrors candidats inclut un préfixe
    // spécifique par layer (`current_texture_a` pour tex_a_source, `current_texture_b`
    // pour tex_b_source) pour permettre à un nœud upstream qui expose les DEUX layers
    // côte à côte (TextureSetNode, Lot AV) de router la bonne texture vers chaque port.
    // Si ces mirrors spécifiques ne sont pas présents, on retombe sur les mirrors
    // génériques utilisés par TextureCycleNode / Grayscale / Spectrogram (compat S5).
    auto pullSourceTexture = [this](const char* portName) -> std::string {
        auto* animator = Animator::instance();
        if (!animator) return std::string();
        auto& in = getInputs();
        auto it = in.find(portName);
        if (it == in.end()) return std::string();
        // Le mirror "spécifique layer" prend la priorité quand il est non vide,
        // sinon on fallback aux mirrors génériques.
        const char* layerSpecific = nullptr;
        if      (std::string(portName) == "tex_a_source") layerSpecific = "current_texture_a";
        else if (std::string(portName) == "tex_b_source") layerSpecific = "current_texture_b";
        for (auto* src : animator->getSourceNodes(it->second)) {
            if (!src) continue;
            auto* sp = src->getParamSpec();
            if (!sp) continue;
            if (layerSpecific) {
                auto* p = sp->getParam(layerSpecific);
                if (p && !p->stringVal.empty()) return p->stringVal;
            }
            for (const char* m : {"current_texture", "texture_out", "texture", "material_out"}) {
                auto* p = sp->getParam(m);
                if (p && !p->stringVal.empty()) return p->stringVal;
            }
        }
        return std::string();
    };
    std::string texAName = pullSourceTexture("tex_a_source");
    if (texAName.empty() && texAParam) texAName = texAParam->stringVal;
    std::string texBName = pullSourceTexture("tex_b_source");
    if (texBName.empty() && texBParam) texBName = texBParam->stringVal;

    if (!texAName.empty() && texAName != mPrevTexA) {
        pass->getTextureUnitState(0)->setTextureName(texAName);
        mPrevTexA = texAName;
    }
    if (!texBName.empty() && texBName != mPrevTexB) {
        pass->getTextureUnitState(2)->setTextureName(texBName);   // layer B = TUS 2
        mPrevTexB = texBName;
    }
    if (maskParam && maskParam->stringVal != mPrevMask && !maskParam->stringVal.empty()) {
        pass->getTextureUnitState(1)->setTextureName(maskParam->stringVal);   // mask = TUS 1
        mPrevMask = maskParam->stringVal;
    }

    // Sync blend_mode from ParamSpec
    auto* bmParam = mSpec.getParam("blend_mode");
    if (bmParam) {
        BlendMode requested = BlendMode::Alpha;
        if      (bmParam->stringVal == "add")      requested = BlendMode::Add;
        else if (bmParam->stringVal == "multiply") requested = BlendMode::Multiply;
        else if (bmParam->stringVal == "screen")   requested = BlendMode::Screen;
        else                                        requested = BlendMode::Alpha;
        mBlendMode = requested;
    }
    if (mBlendMode != mPrevBlendMode) {
        applyBlendMode();
        mPrevBlendMode = mBlendMode;
    }

    // Apply per-layer animation transforms (delta-detected internally).
    auto& in = getInputs();
    auto getf = [&in](const char* name, float def) {
        auto it = in.find(name);
        return (it != in.end()) ? it->second->getValue() : def;
    };

    // Lot AU.23 — integrate the *_speed ports over dt (= the 2006 TimePulseController).
    // Gated by `mIntegrationArmed` (set by onFrameAdvance() — guaranteed once-per-frame entry
    // from tick()) so re-entrant update() calls from the DAG propagation cascade don't
    // double-count dt. The TUS scroll/rotate/mask-offset = accumulator + static port (additive).
    // OGRE wraps UVs naturally with TAM_WRAP, so the accumulators can grow unboundedly.
    if (mIntegrationArmed) {
        mIntegrationArmed = false;
        const float dt = getf("dt", 1.0f / 60.0f);

        // Lot AV — factor (multiplicateur layer B) + amps (clamp ±) à appliquer
        // AVANT intégration. Reproduction des `setSpeedBounds(-2u, 2u, -2v, 2v)`
        // + `setSpeed(u[i], v[i]) où u[2] = u[1] * factor` du 2006.
        const float factor = getf("factor", 1.0f);
        const float u_amp = std::max(0.0f, getf("u_amp", 10.0f));
        const float v_amp = std::max(0.0f, getf("v_amp", 10.0f));
        auto clamp_speed = [](float v, float amp) {
            if (v >  amp) return  amp;
            if (v < -amp) return -amp;
            return v;
        };
        const float spd_ua = clamp_speed(getf("scroll_u_a_speed", 0.0f),          u_amp);
        const float spd_va = clamp_speed(getf("scroll_v_a_speed", 0.0f),          v_amp);
        const float spd_ub = clamp_speed(getf("scroll_u_b_speed", 0.0f) * factor, u_amp);
        const float spd_vb = clamp_speed(getf("scroll_v_b_speed", 0.0f) * factor, v_amp);

        mScrollAccumUA += dt * spd_ua;
        mScrollAccumVA += dt * spd_va;
        mScrollAccumUB += dt * spd_ub;
        mScrollAccumVB += dt * spd_vb;
        mRotateAccumA  += dt * getf("rotate_a_speed",   0.0f);
        mRotateAccumB  += dt * getf("rotate_b_speed",   0.0f);
        mMaskAccumU    += dt * getf("mask_offset_u_speed", 0.0f);
        mMaskAccumV    += dt * getf("mask_offset_v_speed", 0.0f);
        static int s_dbg_n = 0;
        if (s_dbg_n++ % 60 == 0) {
            BBFX_DLOG("[BLD " << getName() << "] dt=" << dt
                      << " spdA=" << getf("scroll_u_a_speed", 0.0f)
                      << " spdB=" << getf("scroll_u_b_speed", 0.0f)
                      << " spdMV=" << getf("mask_offset_v_speed", 0.0f)
                      << " accUA=" << mScrollAccumUA
                      << " accUB=" << mScrollAccumUB
                      << " accMV=" << mMaskAccumV
                      << " mat=" << mGeneratedMaterialName
                      << " texA=" << mPrevTexA << " texB=" << mPrevTexB);
        }
    }

    applyTUSTransforms(0,
        mScrollAccumUA + getf("scroll_u_a", 0), mScrollAccumVA + getf("scroll_v_a", 0),
        mRotateAccumA  + getf("rotate_a",   0),
        getf("tex_scale_u_a", 1), getf("tex_scale_v_a", 1),
        mPrevScrollUA, mPrevScrollVA, mPrevRotateA, mPrevScaleUA, mPrevScaleVA);

    applyTUSTransforms(2,                               // layer B = TUS 2
        mScrollAccumUB + getf("scroll_u_b", 0), mScrollAccumVB + getf("scroll_v_b", 0),
        mRotateAccumB  + getf("rotate_b",   0),
        getf("tex_scale_u_b", 1), getf("tex_scale_v_b", 1),
        mPrevScrollUB, mPrevScrollVB, mPrevRotateB, mPrevScaleUB, mPrevScaleVB);

    // Mask offset (U = horizontal slide, V = the historic vertical sweep) — mask = TUS 1.
    float maskOffU = mMaskAccumU + getf("mask_offset_u", 0.0f);
    float maskOffV = mMaskAccumV + getf("mask_offset_v", 0.0f);
    if (maskOffU != mPrevMaskOffsetU || maskOffV != mPrevMaskOffsetV) {
        auto* tusM = pass->getTextureUnitState(1);
        tusM->setTextureScroll(maskOffU, maskOffV);
        mPrevMaskOffsetU = maskOffU;
        mPrevMaskOffsetV = maskOffV;
    }

    // Mask invert via address mode toggle (mirror vs wrap on the mask layer = TUS 1).
    auto* miParam = mSpec.getParam("mask_invert");
    bool invert = (miParam ? miParam->boolVal : false);
    if (invert != mPrevMaskInvert) {
        auto* tusM = pass->getTextureUnitState(1);
        Ogre::TextureUnitState::UVWAddressingMode am{};
        am.u = invert ? Ogre::TextureUnitState::TAM_MIRROR : Ogre::TextureUnitState::TAM_WRAP;
        am.v = am.u;
        am.w = am.u;
        tusM->setTextureAddressingMode(am);
        mPrevMaskInvert = invert;
    }

    // Lot AV.2 (I-2050 root-cause fix) — RTSS (RTShaderSystem) compile un shader-based
    // technique alternative à partir du fixed-function pipeline au PREMIER render du
    // matériau (cf. core/Engine.cpp `ShaderGeneratorResolver::handleSchemeNotFound`).
    // Les mutations TUS subséquentes (setTextureScroll, setTextureName, setTextureScale,
    // setColourOperationEx via applyBlendMode) ne sont PAS reflétées dans la technique
    // RTSS générée → matériau visuellement figé sur snapshot premier bind. Reproduit
    // sur tous les consumers (Rectangle2D NDC, BillboardSet camera-node, SceneObjectNode
    // 3D). Fix : invalider la technique RTSS chaque frame après les mutations TUS — OGRE
    // re-générera le shader au prochain render avec les valeurs courantes.
    //
    // Coût : 1 invalidation/frame par TextureBlendNode actif. RTSS re-compile à la
    // demande mais cache le résultat ; en pratique seul le `tex_matrix` auto-param
    // change donc la re-compilation est rapide. Acceptable pour le scope VJ courant ;
    // optimisation possible (n'invalider que si une TUS a réellement changé via les
    // delta-checks de applyTUSTransforms) reportée si profilage le justifie.
    if (auto* sg = Ogre::RTShader::ShaderGenerator::getSingletonPtr()) {
        sg->invalidateMaterial(Ogre::RTShader::ShaderGenerator::DEFAULT_SCHEME_NAME,
                               mGeneratedMaterialName,
                               mGeneratedMaterial->getGroup());
    }

    // Material ready flag
    auto& out = getOutputs();
    auto itReady = out.find("material_ready");
    if (itReady != out.end()) itReady->second->setValue(1.0f);

    fireUpdate();
}

void TextureBlendNode::cleanup() {
    if (!mGeneratedMaterial.isNull()) {
        Ogre::MaterialManager::getSingleton().remove(mGeneratedMaterialName);
        mGeneratedMaterial.setNull();
    }
}

} // namespace bbfx
