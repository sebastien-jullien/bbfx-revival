#include "VideoExporter.h"
#include <OgreRenderWindow.h>
#include <iostream>
#include <sstream>
#include <iomanip>
#include <filesystem>

namespace bbfx {

void VideoExporter::start(const std::string& outputDir) {
    mOutputDir = outputDir;
    mFrameCount = 0;

    // Create output directory if it doesn't exist
    std::filesystem::create_directories(outputDir);

    mExporting.store(true);
    std::cout << "[VideoExporter] Exporting to " << outputDir << "/" << std::endl;
}

void VideoExporter::captureFrame(Ogre::RenderWindow* renderWindow) {
    if (!mExporting.load() || !renderWindow) return;

    mFrameCount++;
    std::ostringstream filename;
    filename << mOutputDir << "/frame_"
             << std::setw(6) << std::setfill('0') << mFrameCount
             << ".png";

    renderWindow->writeContentsToFile(filename.str());
}

void VideoExporter::stop() {
    if (!mExporting.load()) return;
    mExporting.store(false);
    std::cout << "[VideoExporter] Exported " << mFrameCount << " frames to " << mOutputDir << "/" << std::endl;
}

} // namespace bbfx
