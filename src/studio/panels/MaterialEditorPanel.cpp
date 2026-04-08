#include "MaterialEditorPanel.h"
#include "../ShaderPreviewRenderer.h"
#include "../TextureThumbnailCache.h"

#include <Ogre.h>
#include <imgui.h>
#include <iostream>
#include <cstring>

namespace bbfx {

MaterialEditorPanel::MaterialEditorPanel() {}

void MaterialEditorPanel::setMaterial(const std::string& materialName) {
    if (materialName == mMaterialName) return;
    mMaterialName = materialName;
    loadFromMaterial();
}

void MaterialEditorPanel::loadFromMaterial() {
    if (mMaterialName.empty()) return;
    try {
        Ogre::MaterialPtr mat = Ogre::MaterialManager::getSingleton().getByName(mMaterialName,
            Ogre::ResourceGroupManager::AUTODETECT_RESOURCE_GROUP_NAME);
        if (!mat || !mat->isLoaded()) return;

        Ogre::Technique* tech = mat->getBestTechnique();
        if (!tech || tech->getNumPasses() == 0) return;

        Ogre::Pass* pass = tech->getPass(0);
        Ogre::ColourValue d = pass->getDiffuse();
        mDiffuse[0] = d.r; mDiffuse[1] = d.g; mDiffuse[2] = d.b; mAlpha = d.a;
        Ogre::ColourValue s = pass->getSpecular();
        mSpecular[0] = s.r; mSpecular[1] = s.g; mSpecular[2] = s.b;
        Ogre::ColourValue a = pass->getAmbient();
        mAmbient[0] = a.r; mAmbient[1] = a.g; mAmbient[2] = a.b;
        Ogre::ColourValue e = pass->getSelfIllumination();
        mEmissive[0] = e.r; mEmissive[1] = e.g; mEmissive[2] = e.b;
        mShininess = pass->getShininess();
    } catch (...) {}
}

void MaterialEditorPanel::applyToMaterial() {
    if (mMaterialName.empty()) return;
    try {
        Ogre::MaterialPtr mat = Ogre::MaterialManager::getSingleton().getByName(mMaterialName,
            Ogre::ResourceGroupManager::AUTODETECT_RESOURCE_GROUP_NAME);
        if (!mat || !mat->isLoaded()) return;

        Ogre::Technique* tech = mat->getBestTechnique();
        if (!tech || tech->getNumPasses() == 0) return;

        Ogre::Pass* pass = tech->getPass(0);
        pass->setDiffuse(mDiffuse[0], mDiffuse[1], mDiffuse[2], mAlpha);
        pass->setSpecular(mSpecular[0], mSpecular[1], mSpecular[2], 1.0f);
        pass->setAmbient(mAmbient[0], mAmbient[1], mAmbient[2]);
        pass->setSelfIllumination(mEmissive[0], mEmissive[1], mEmissive[2]);
        pass->setShininess(mShininess);
    } catch (...) {}
}

void MaterialEditorPanel::render() {
    ImGui::Begin("Material Editor");

    if (mMaterialName.empty()) {
        ImGui::TextDisabled("No material selected");
        ImGui::TextDisabled("Double-click a material in the browser to open it here.");
        ImGui::End();
        return;
    }

    // Header
    ImGui::TextColored({0.0f, 1.0f, 0.8f, 1.0f}, "Material: %s", mMaterialName.c_str());
    ImGui::Separator();

    // Preview sphere
    if (mRenderer && mRenderer->isInitialized()) {
        ImTextureID preview = mRenderer->getMaterialPreview(mMaterialName);
        ImGui::Image(preview, {128, 128});
        ImGui::Separator();
    }

    // Colors section
    ImGui::TextDisabled("Colors");
    bool changed = false;
    changed |= ImGui::ColorEdit3("Diffuse", mDiffuse);
    changed |= ImGui::ColorEdit3("Specular", mSpecular);
    changed |= ImGui::ColorEdit3("Ambient", mAmbient);
    changed |= ImGui::ColorEdit3("Emissive", mEmissive);
    changed |= ImGui::SliderFloat("Shininess", &mShininess, 0.0f, 128.0f);
    changed |= ImGui::SliderFloat("Alpha", &mAlpha, 0.0f, 1.0f);

    if (changed) {
        applyToMaterial();
    }

    ImGui::Separator();

    // Texture slots
    ImGui::TextDisabled("Textures");
    try {
        Ogre::MaterialPtr mat = Ogre::MaterialManager::getSingleton().getByName(mMaterialName,
            Ogre::ResourceGroupManager::AUTODETECT_RESOURCE_GROUP_NAME);
        if (mat && mat->isLoaded()) {
            Ogre::Technique* tech = mat->getBestTechnique();
            if (tech && tech->getNumPasses() > 0) {
                Ogre::Pass* pass = tech->getPass(0);
                int numTex = static_cast<int>(pass->getNumTextureUnitStates());
                if (numTex == 0) {
                    ImGui::TextDisabled("No texture slots");
                } else {
                    for (int i = 0; i < numTex; ++i) {
                        Ogre::TextureUnitState* tus = pass->getTextureUnitState(i);
                        std::string texName = tus->getTextureName();
                        ImGui::PushID(i);

                        // Thumbnail
                        if (mThumbCache && !texName.empty()) {
                            ImTextureID thumb = mThumbCache->getThumbnail(texName);
                            ImGui::Image(thumb, {48, 48});
                            ImGui::SameLine();
                        }

                        ImGui::Text("Slot %d: %s", i, texName.empty() ? "(none)" : texName.c_str());
                        ImGui::PopID();
                    }
                }
            }
        }
    } catch (...) {}

    // Create new material button
    ImGui::Separator();
    static char newMatBuf[128] = {};
    ImGui::SetNextItemWidth(200);
    ImGui::InputText("##newmat", newMatBuf, sizeof(newMatBuf));
    ImGui::SameLine();
    if (ImGui::Button("New Material")) {
        std::string newName(newMatBuf);
        if (!newName.empty()) {
            try {
                Ogre::MaterialManager::getSingleton().create(newName,
                    Ogre::ResourceGroupManager::DEFAULT_RESOURCE_GROUP_NAME);
                setMaterial(newName);
                std::cout << "[MaterialEditor] Created material: " << newName << std::endl;
            } catch (const std::exception& e) {
                std::cerr << "[MaterialEditor] Failed: " << e.what() << std::endl;
            }
            std::memset(newMatBuf, 0, sizeof(newMatBuf));
        }
    }

    ImGui::End();
}

} // namespace bbfx
