#include "SkyboxNode.h"
#include <OgreMaterialManager.h>
#include <OgreTechnique.h>
#include <OgrePass.h>
#include <OgreTextureManager.h>
#include <OgreHardwarePixelBuffer.h>
#include <OgreColourValue.h>
#include <cstdint>
#include <algorithm>

namespace bbfx {

static int sSkyCounter = 0;

SkyboxNode::SkyboxNode(const std::string& name, Ogre::SceneManager* scene)
    : AnimationNode(name), mScene(scene) {
    ParamDef typeDef; typeDef.name = "sky_type"; typeDef.label = "Type"; typeDef.type = ParamType::ENUM;
    typeDef.stringVal = "color"; typeDef.choices = {"skybox", "color", "gradient"};
    mSpec.addParam(typeDef);
    ParamDef bgDef; bgDef.name = "bg_color"; bgDef.label = "Background"; bgDef.type = ParamType::COLOR;
    bgDef.colorVal[0] = 0.05f; bgDef.colorVal[1] = 0.05f; bgDef.colorVal[2] = 0.1f;
    mSpec.addParam(bgDef);
    // D11 — matériau de cubemap pour le type "skybox" (ressources BBFx/*SkyBox).
    ParamDef matDef; matDef.name = "material"; matDef.label = "Skybox Material"; matDef.type = ParamType::STRING;
    matDef.stringVal = "BBFx/StormySkyBox";
    mSpec.addParam(matDef);
    setParamSpec(&mSpec);
    addInput(new AnimationPort("rotation", 0.0f));

    std::string id = std::to_string(++sSkyCounter);
    mColorMatName    = "SkyboxNode_color_"   + id;
    mGradientMatName = "SkyboxNode_grad_"     + id;
    mGradientTexName = "SkyboxNode_gradtex_"  + id;
}

SkyboxNode::~SkyboxNode() {
    // C7 — si ce node pilotait encore le ciel, le désactiver pour ne pas laisser
    // la scène pointer un matériau qu'on s'apprête à supprimer (skybox/skydome figé).
    if (mScene && !mAppliedType.empty()) {
        if (mScene->isSkyBoxEnabled())  mScene->setSkyBox(false, Ogre::BLANKSTRING);
        if (mScene->isSkyDomeEnabled()) mScene->setSkyDome(false, Ogre::BLANKSTRING);
    }
    // Libère les matériaux/texture créés au runtime (sinon fuite à chaque cycle create/delete).
    auto& mm = Ogre::MaterialManager::getSingleton();
    if (!mColorMatName.empty()    && mm.getByName(mColorMatName,    Ogre::ResourceGroupManager::DEFAULT_RESOURCE_GROUP_NAME)) mm.remove(mColorMatName,    Ogre::ResourceGroupManager::DEFAULT_RESOURCE_GROUP_NAME);
    if (!mGradientMatName.empty() && mm.getByName(mGradientMatName, Ogre::ResourceGroupManager::DEFAULT_RESOURCE_GROUP_NAME)) mm.remove(mGradientMatName, Ogre::ResourceGroupManager::DEFAULT_RESOURCE_GROUP_NAME);
    auto& tm = Ogre::TextureManager::getSingleton();
    if (!mGradientTexName.empty() && tm.getByName(mGradientTexName, Ogre::ResourceGroupManager::DEFAULT_RESOURCE_GROUP_NAME)) tm.remove(mGradientTexName, Ogre::ResourceGroupManager::DEFAULT_RESOURCE_GROUP_NAME);
}

void SkyboxNode::ensureColorMaterial(float r, float g, float b) {
    auto& mm = Ogre::MaterialManager::getSingleton();
    Ogre::MaterialPtr mat = mm.getByName(mColorMatName, Ogre::ResourceGroupManager::DEFAULT_RESOURCE_GROUP_NAME);
    if (!mat) {
        mat = mm.create(mColorMatName, Ogre::ResourceGroupManager::DEFAULT_RESOURCE_GROUP_NAME);
        auto* pass = mat->getTechnique(0)->getPass(0);
        pass->setLightingEnabled(false);
        pass->setDepthWriteEnabled(false);
        pass->setCullingMode(Ogre::CULL_NONE);
    }
    auto* pass = mat->getTechnique(0)->getPass(0);
    // Couleur unie : émissive = bg_color (lighting off → rendu plat de la couleur).
    pass->setSelfIllumination(Ogre::ColourValue(r, g, b));
    pass->setAmbient(Ogre::ColourValue(r, g, b));
    pass->setDiffuse(Ogre::ColourValue(r, g, b));
}

void SkyboxNode::ensureGradientMaterial(float r, float g, float b) {
    auto& tm = Ogre::TextureManager::getSingleton();
    Ogre::TexturePtr tex = tm.getByName(mGradientTexName, Ogre::ResourceGroupManager::DEFAULT_RESOURCE_GROUP_NAME);
    if (!tex) {
        tex = tm.createManual(mGradientTexName, Ogre::ResourceGroupManager::DEFAULT_RESOURCE_GROUP_NAME,
                              Ogre::TEX_TYPE_2D, 2, 64, 0, Ogre::PF_BYTE_RGBA, Ogre::TU_DYNAMIC_WRITE_ONLY);
    }
    // Remplit un dégradé vertical : haut = bg_color, bas = bg_color * 0.25 (horizon sombre).
    auto buf = tex->getBuffer();
    buf->lock(Ogre::HardwareBuffer::HBL_DISCARD);
    const Ogre::PixelBox& pb = buf->getCurrentLock();
    auto* dst = static_cast<uint8_t*>(pb.data);
    size_t H = tex->getHeight(), W = tex->getWidth();
    size_t rowPitch = pb.rowPitch * 4; // PF_BYTE_RGBA = 4 bytes
    for (size_t y = 0; y < H; ++y) {
        float t = static_cast<float>(y) / static_cast<float>(H - 1); // 0 haut → 1 bas
        float k = 1.0f - 0.75f * t;
        uint8_t cr = static_cast<uint8_t>(std::min(1.0f, r * k) * 255.0f);
        uint8_t cg = static_cast<uint8_t>(std::min(1.0f, g * k) * 255.0f);
        uint8_t cb = static_cast<uint8_t>(std::min(1.0f, b * k) * 255.0f);
        for (size_t x = 0; x < W; ++x) {
            uint8_t* px = dst + y * rowPitch + x * 4;
            px[0] = cr; px[1] = cg; px[2] = cb; px[3] = 255;
        }
    }
    buf->unlock();

    auto& mm = Ogre::MaterialManager::getSingleton();
    Ogre::MaterialPtr mat = mm.getByName(mGradientMatName, Ogre::ResourceGroupManager::DEFAULT_RESOURCE_GROUP_NAME);
    if (!mat) {
        mat = mm.create(mGradientMatName, Ogre::ResourceGroupManager::DEFAULT_RESOURCE_GROUP_NAME);
        auto* pass = mat->getTechnique(0)->getPass(0);
        pass->setLightingEnabled(false);
        pass->setDepthWriteEnabled(false);
        pass->createTextureUnitState(mGradientTexName);
    }
}

void SkyboxNode::update() {
    if (!mScene) { fireUpdate(); return; }

    auto* typeParam = mSpec.getParam("sky_type");
    auto* bgParam   = mSpec.getParam("bg_color");
    auto* matParam  = mSpec.getParam("material");
    std::string type = typeParam ? typeParam->stringVal : "color";
    float r = bgParam ? bgParam->colorVal[0] : 0.05f;
    float g = bgParam ? bgParam->colorVal[1] : 0.05f;
    float b = bgParam ? bgParam->colorVal[2] : 0.1f;
    std::string skyMat = matParam ? matParam->stringVal : "BBFx/StormySkyBox";

    // D11 — ne reconstruire/appliquer qu'au changement réel (type, couleur, matériau).
    bool dirty = (type != mAppliedType) || (skyMat != mAppliedMaterial) ||
                 (r != mAppliedR) || (g != mAppliedG) || (b != mAppliedB);
    if (dirty) {
        if (type == "skybox") {
            mScene->setSkyDome(false, Ogre::BLANKSTRING);
            mScene->setSkyBox(true, skyMat, 5000.0f);
        } else if (type == "gradient") {
            ensureGradientMaterial(r, g, b);
            mScene->setSkyBox(false, Ogre::BLANKSTRING);
            mScene->setSkyDome(true, mGradientMatName, 10.0f, 8.0f);
        } else { // "color"
            ensureColorMaterial(r, g, b);
            mScene->setSkyDome(false, Ogre::BLANKSTRING);
            mScene->setSkyBox(true, mColorMatName, 5000.0f);
        }
        mAppliedType = type; mAppliedMaterial = skyMat;
        mAppliedR = r; mAppliedG = g; mAppliedB = b;
    }

    // Rotation du ciel (maintenant que le skybox/skydome existe réellement).
    auto& ins = getInputs();
    auto rotIt = ins.find("rotation");
    if (rotIt != ins.end()) {
        float rot = rotIt->second->getValue();
        if (auto* skyNode = mScene->getSkyBoxNode())
            skyNode->setOrientation(Ogre::Quaternion(Ogre::Degree(rot), Ogre::Vector3::UNIT_Y));
        if (auto* domeNode = mScene->getSkyDomeNode())
            domeNode->setOrientation(Ogre::Quaternion(Ogre::Degree(rot), Ogre::Vector3::UNIT_Y));
    }

    fireUpdate();
}
} // namespace bbfx
