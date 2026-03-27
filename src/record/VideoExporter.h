#pragma once

#include <string>
#include <atomic>

namespace Ogre { class RenderWindow; }

namespace bbfx {

/// Captures each rendered frame as a PNG file in a sequence.
/// Uses Ogre::RenderTarget::writeContentsToFile().
class VideoExporter {
public:
    VideoExporter() = default;

    void start(const std::string& outputDir);
    void captureFrame(Ogre::RenderWindow* renderWindow);
    void stop();

    bool isExporting() const { return mExporting.load(); }
    int getFrameCount() const { return mFrameCount; }

private:
    std::string mOutputDir;
    int mFrameCount = 0;
    std::atomic<bool> mExporting{false};
};

} // namespace bbfx
