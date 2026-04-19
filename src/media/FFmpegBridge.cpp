#include "FFmpegBridge.h"

#include <chrono>
#include <cstring>
#include <filesystem>
#include <iostream>
#include <sstream>

#include <OgreHardwarePixelBuffer.h>
#include <OgrePixelFormat.h>
#include <OgreTexture.h>
#include <OgreTextureManager.h>

#ifdef _WIN32
#  ifndef NOMINMAX
#    define NOMINMAX
#  endif
#  include <windows.h>
#else
#  include <fcntl.h>
#  include <signal.h>
#  include <sys/wait.h>
#  include <unistd.h>
#endif

namespace bbfx {

namespace {

bool fileExists(const std::string& p) {
    std::error_code ec;
    return std::filesystem::exists(p, ec);
}

} // anonymous

FFmpegBridge::FFmpegBridge() { mFfmpegPath = findFFmpegBinary(); }

FFmpegBridge::~FFmpegBridge() { close(); }

std::string FFmpegBridge::findFFmpegBinary() {
#ifdef _WIN32
    static const char* kFallbacks[] = {
        "ffmpeg.exe",
        "C:\\ffmpeg\\bin\\ffmpeg.exe",
        "C:\\Program Files\\ffmpeg\\bin\\ffmpeg.exe",
        "C:\\Program Files (x86)\\ffmpeg\\bin\\ffmpeg.exe",
    };
#else
    static const char* kFallbacks[] = {
        "ffmpeg",
        "/usr/bin/ffmpeg",
        "/usr/local/bin/ffmpeg",
        "/opt/homebrew/bin/ffmpeg",
    };
#endif
    for (auto* p : kFallbacks) {
        if (fileExists(p)) return p;
    }
    // Fallback to the name — OS will search PATH when CreateProcess /
    // execvp runs.
#ifdef _WIN32
    return "ffmpeg.exe";
#else
    return "ffmpeg";
#endif
}

bool FFmpegBridge::open(const std::string& source, int width, int height, double fps) {
    if (mRunning) close();
    mSource = source;
    mWidth  = (width  > 0) ? width  : 1280;
    mHeight = (height > 0) ? height : 720;
    mFps    = (fps    > 0.0) ? fps  : 30.0;
    mPosition = 0.0;
    mFrameAccum = 0.0;

    // Create the OGRE texture up-front (empty). Even if ffmpeg is
    // missing the caller can still query getTextureName() without
    // seg-faulting; the texture will just be black.
    auto& tm = Ogre::TextureManager::getSingleton();
    std::string stem = std::filesystem::path(source).stem().string();
    if (stem.empty()) stem = "clip";
    mTextureName = "bbfx_ff_" + stem + "_" + std::to_string(
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count());
    try {
        if (auto existing = tm.getByName(mTextureName); existing) tm.remove(existing);
        tm.createManual(mTextureName,
                         Ogre::ResourceGroupManager::DEFAULT_RESOURCE_GROUP_NAME,
                         Ogre::TEX_TYPE_2D, mWidth, mHeight, 0,
                         Ogre::PF_A8R8G8B8, Ogre::TU_DEFAULT);
    } catch (const std::exception& e) {
        std::cerr << "[FFmpegBridge] texture create failed: " << e.what() << std::endl;
    }

    if (!fileExists(mFfmpegPath) && mFfmpegPath.find('\\') == std::string::npos &&
        mFfmpegPath.find('/') == std::string::npos) {
        // `mFfmpegPath` is just a name — let the OS resolve via PATH
        // when we spawn. Nothing to do here.
    } else if (!fileExists(mFfmpegPath)) {
        std::cerr << "[FFmpegBridge] ffmpeg binary not found at '"
                   << mFfmpegPath << "' — the clip will not play." << std::endl;
        return false;
    }

    if (!startSubprocess(source, 0.0)) {
        return false;
    }
    mRunning = true;
    mReaderThread = std::thread([this]() { readerLoop(); });
    return true;
}

void FFmpegBridge::close() {
    mPlaying = false;
    mRunning = false;
    killSubprocess();
    if (mReaderThread.joinable()) mReaderThread.join();
    std::lock_guard<std::mutex> lock(mFrameMutex);
    mPendingFrame.clear();
    mFramePending = false;
}

bool FFmpegBridge::startSubprocess(const std::string& source, double startSeconds) {
#ifdef _WIN32
    SECURITY_ATTRIBUTES sa{};
    sa.nLength = sizeof(sa);
    sa.bInheritHandle = TRUE;
    HANDLE readH = nullptr, writeH = nullptr;
    if (!CreatePipe(&readH, &writeH, &sa, 1 << 20)) {
        std::cerr << "[FFmpegBridge] CreatePipe failed" << std::endl;
        return false;
    }
    SetHandleInformation(readH, HANDLE_FLAG_INHERIT, 0);

    std::ostringstream cmd;
    cmd << "\"" << mFfmpegPath << "\""
        << " -hide_banner -loglevel error"
        << " -ss " << startSeconds
        << " -i \"" << source << "\""
        << " -f rawvideo -pix_fmt rgba"
        << " -s " << mWidth << "x" << mHeight
        << " -r " << mFps
        << " -";
    std::string cmdLine = cmd.str();

    STARTUPINFOA si{};
    si.cb = sizeof(si);
    si.hStdOutput = writeH;
    si.hStdError  = GetStdHandle(STD_ERROR_HANDLE);
    si.hStdInput  = GetStdHandle(STD_INPUT_HANDLE);
    si.dwFlags    = STARTF_USESTDHANDLES;
    PROCESS_INFORMATION pi{};

    std::vector<char> mut(cmdLine.begin(), cmdLine.end()); mut.push_back(0);
    BOOL ok = CreateProcessA(nullptr, mut.data(), nullptr, nullptr, TRUE,
                              CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi);
    CloseHandle(writeH);
    if (!ok) {
        CloseHandle(readH);
        std::cerr << "[FFmpegBridge] CreateProcess failed (ffmpeg not on PATH?)" << std::endl;
        return false;
    }
    CloseHandle(pi.hThread);
    mProcessHandle = reinterpret_cast<uintptr_t>(pi.hProcess);
    mStdoutRead    = reinterpret_cast<uintptr_t>(readH);
    return true;
#else
    int pipefd[2];
    if (pipe(pipefd) != 0) return false;
    pid_t pid = fork();
    if (pid < 0) { ::close(pipefd[0]); ::close(pipefd[1]); return false; }
    if (pid == 0) {
        // Child.
        dup2(pipefd[1], STDOUT_FILENO);
        ::close(pipefd[0]);
        ::close(pipefd[1]);
        char ss[32]; std::snprintf(ss, sizeof(ss), "%.3f", startSeconds);
        char sz[32]; std::snprintf(sz, sizeof(sz), "%dx%d", mWidth, mHeight);
        char fps[16]; std::snprintf(fps, sizeof(fps), "%.3f", mFps);
        char* argv[] = {
            const_cast<char*>(mFfmpegPath.c_str()),
            const_cast<char*>("-hide_banner"),
            const_cast<char*>("-loglevel"), const_cast<char*>("error"),
            const_cast<char*>("-ss"), ss,
            const_cast<char*>("-i"), const_cast<char*>(source.c_str()),
            const_cast<char*>("-f"), const_cast<char*>("rawvideo"),
            const_cast<char*>("-pix_fmt"), const_cast<char*>("rgba"),
            const_cast<char*>("-s"), sz,
            const_cast<char*>("-r"), fps,
            const_cast<char*>("-"), nullptr
        };
        execvp(argv[0], argv);
        _exit(127);
    }
    ::close(pipefd[1]);
    mProcessHandle = static_cast<uintptr_t>(pid);
    mStdoutRead    = static_cast<uintptr_t>(pipefd[0]);
    return true;
#endif
}

void FFmpegBridge::killSubprocess() {
#ifdef _WIN32
    if (mProcessHandle) {
        TerminateProcess(reinterpret_cast<HANDLE>(mProcessHandle), 0);
        WaitForSingleObject(reinterpret_cast<HANDLE>(mProcessHandle), 1000);
        CloseHandle(reinterpret_cast<HANDLE>(mProcessHandle));
        mProcessHandle = 0;
    }
    if (mStdoutRead) {
        CloseHandle(reinterpret_cast<HANDLE>(mStdoutRead));
        mStdoutRead = 0;
    }
#else
    if (mProcessHandle) {
        kill(static_cast<pid_t>(mProcessHandle), SIGKILL);
        int status = 0;
        waitpid(static_cast<pid_t>(mProcessHandle), &status, 0);
        mProcessHandle = 0;
    }
    if (mStdoutRead) {
        ::close(static_cast<int>(mStdoutRead));
        mStdoutRead = 0;
    }
#endif
}

void FFmpegBridge::readerLoop() {
    const size_t frameBytes = static_cast<size_t>(mWidth) * mHeight * 4;
    std::vector<uint8_t> buf(frameBytes);
    while (mRunning) {
        size_t got = 0;
        while (got < frameBytes && mRunning) {
#ifdef _WIN32
            DWORD n = 0;
            BOOL ok = ReadFile(reinterpret_cast<HANDLE>(mStdoutRead),
                                buf.data() + got,
                                static_cast<DWORD>(frameBytes - got),
                                &n, nullptr);
            if (!ok || n == 0) { mRunning = false; break; }
            got += n;
#else
            ssize_t n = ::read(static_cast<int>(mStdoutRead),
                                buf.data() + got, frameBytes - got);
            if (n <= 0) { mRunning = false; break; }
            got += static_cast<size_t>(n);
#endif
        }
        if (got < frameBytes) {
            // EOF — loop if configured.
            if (mLoop && !mSource.empty()) {
                killSubprocess();
                if (startSubprocess(mSource, 0.0)) {
                    mRunning = true;
                    mPosition = 0.0;
                    continue;
                }
            }
            mRunning = false;
            break;
        }
        std::lock_guard<std::mutex> lock(mFrameMutex);
        mPendingFrame = buf;
        mFramePending = true;
    }
}

void FFmpegBridge::update(float /*dt*/) {
    if (!mFramePending) return;
    std::vector<uint8_t> frame;
    {
        std::lock_guard<std::mutex> lock(mFrameMutex);
        if (!mFramePending) return;
        frame.swap(mPendingFrame);
        mFramePending = false;
    }
    auto& tm = Ogre::TextureManager::getSingleton();
    auto tex = tm.getByName(mTextureName);
    if (!tex) return;
    auto pb = tex->getBuffer();
    try {
        pb->lock(Ogre::HardwareBuffer::HBL_DISCARD);
        auto box = pb->getCurrentLock();
        // Source RGBA, dst is PF_A8R8G8B8 (BGRA in memory on LE).
        const uint8_t* src = frame.data();
        uint8_t* dst = reinterpret_cast<uint8_t*>(box.data);
        size_t px = static_cast<size_t>(mWidth) * mHeight;
        for (size_t i = 0; i < px; ++i) {
            dst[i*4 + 0] = src[i*4 + 2];
            dst[i*4 + 1] = src[i*4 + 1];
            dst[i*4 + 2] = src[i*4 + 0];
            dst[i*4 + 3] = src[i*4 + 3];
        }
        pb->unlock();
    } catch (const std::exception& e) {
        std::cerr << "[FFmpegBridge] frame upload failed: " << e.what() << std::endl;
    }
    mPosition += 1.0 / std::max(1.0, mFps);
}

void FFmpegBridge::play()  { mPlaying = true;  }
void FFmpegBridge::pause() { mPlaying = false; }
void FFmpegBridge::stop()  { mPlaying = false; seek(0.0); }

void FFmpegBridge::seek(double seconds) {
    if (!mRunning) return;
    killSubprocess();
    mRunning = false;
    if (mReaderThread.joinable()) mReaderThread.join();
    if (startSubprocess(mSource, std::max(0.0, seconds))) {
        mRunning = true;
        mPosition = seconds;
        mReaderThread = std::thread([this]() { readerLoop(); });
    }
}

void FFmpegBridge::setSpeed(double mult) { if (mult > 0.0) mSpeed = mult; }
void FFmpegBridge::setLoop(bool on)      { mLoop = on; }

} // namespace bbfx
