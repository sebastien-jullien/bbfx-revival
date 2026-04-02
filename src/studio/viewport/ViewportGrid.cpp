#include "ViewportGrid.h"
#include <OgreMaterialManager.h>
#include <OgreTechnique.h>
#include <OgrePass.h>

namespace bbfx {

static constexpr float kGridExtent  = 50.0f;  // ±50m = 100m total
static constexpr float kFineStep    = 1.0f;
static constexpr float kCoarseStep  = 10.0f;

ViewportGrid::ViewportGrid(Ogre::SceneManager* sm)
    : mScene(sm)
{
    createMaterial();
    buildGrid();
}

ViewportGrid::~ViewportGrid()
{
    if (mScene && mGrid) {
        if (mGridNode) {
            mGridNode->detachAllObjects();
            mScene->destroySceneNode(mGridNode);
        }
        mScene->destroyManualObject(mGrid);
    }
}

void ViewportGrid::createMaterial()
{
    auto& mgr = Ogre::MaterialManager::getSingleton();
    if (mgr.resourceExists("bbfx/grid", Ogre::RGN_DEFAULT)) return;

    auto mat = mgr.create("bbfx/grid", Ogre::RGN_DEFAULT);
    auto* pass = mat->getTechnique(0)->getPass(0);
    pass->setLightingEnabled(false);
    pass->setVertexColourTracking(Ogre::TVC_AMBIENT | Ogre::TVC_DIFFUSE);
    pass->setSceneBlending(Ogre::SBT_TRANSPARENT_ALPHA);
    pass->setDepthWriteEnabled(false);
    pass->setDepthCheckEnabled(true);
    pass->setCullingMode(Ogre::CULL_NONE);
}

void ViewportGrid::buildGrid()
{
    mGrid = mScene->createManualObject("bbfx_viewport_grid");
    mGrid->setDynamic(false);
    mGrid->setQueryFlags(0); // not pickable
    mGrid->setRenderQueueGroup(Ogre::RENDER_QUEUE_BACKGROUND);

    mGrid->begin("bbfx/grid", Ogre::RenderOperation::OT_LINE_LIST);

    auto addLine = [&](float x1, float y1, float z1,
                       float x2, float y2, float z2,
                       float r, float g, float b, float a) {
        mGrid->position(x1, y1, z1);
        mGrid->colour(r, g, b, a);
        mGrid->position(x2, y2, z2);
        mGrid->colour(r, g, b, a);
    };

    // Fine grid lines (every 1m)
    for (float i = -kGridExtent; i <= kGridExtent; i += kFineStep) {
        if (std::fabs(i) < 0.01f) continue; // skip origin (drawn as axis)
        if (std::fabs(std::fmod(i, kCoarseStep)) < 0.01f) continue; // skip coarse lines

        float alpha = 0.15f;
        // X-parallel lines (vary Z)
        addLine(-kGridExtent, 0, i, kGridExtent, 0, i, 0.35f, 0.35f, 0.35f, alpha);
        // Z-parallel lines (vary X)
        addLine(i, 0, -kGridExtent, i, 0, kGridExtent, 0.35f, 0.35f, 0.35f, alpha);
    }

    // Coarse grid lines (every 10m)
    for (float i = -kGridExtent; i <= kGridExtent; i += kCoarseStep) {
        if (std::fabs(i) < 0.01f) continue; // skip origin

        float alpha = 0.35f;
        addLine(-kGridExtent, 0, i, kGridExtent, 0, i, 0.5f, 0.5f, 0.5f, alpha);
        addLine(i, 0, -kGridExtent, i, 0, kGridExtent, 0.5f, 0.5f, 0.5f, alpha);
    }

    // X axis (red)
    addLine(-kGridExtent, 0, 0, kGridExtent, 0, 0, 0.8f, 0.2f, 0.2f, 0.6f);
    // Z axis (blue)
    addLine(0, 0, -kGridExtent, 0, 0, kGridExtent, 0.2f, 0.2f, 0.8f, 0.6f);
    // Y axis (green, short vertical)
    addLine(0, 0, 0, 0, 5.0f, 0, 0.2f, 0.8f, 0.2f, 0.6f);

    mGrid->end();

    mGridNode = mScene->getRootSceneNode()->createChildSceneNode("bbfx_grid_node");
    mGridNode->attachObject(mGrid);
}

void ViewportGrid::setVisible(bool v)
{
    mVisible = v;
    if (mGridNode) mGridNode->setVisible(v);
}

} // namespace bbfx
