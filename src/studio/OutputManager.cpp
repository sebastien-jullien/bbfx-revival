#include "OutputManager.h"
#include "SurfaceMap.h"
#include "TextureShareSender.h"
#include <OgreTextureManager.h>
#include <OgreHardwarePixelBuffer.h>
#include <OgreSceneManager.h>
#include <OgreCamera.h>
#include <OgreViewport.h>
#include <OgreColourValue.h>
#include <OgreResourceGroupManager.h>
#include <OgreMaterialManager.h>
#include <OgreCompositorManager.h>
#include <OgreCompositorChain.h>
#include <OgreTechnique.h>
#include <OgrePass.h>
#include <iostream>

#ifdef _WIN32
#  ifndef NOMINMAX
#    define NOMINMAX
#  endif
#  include <windows.h>
#  include <GL/gl.h>
#else
#  include <GL/gl.h>
#endif

namespace bbfx {

// ── Native Win32 output windows (AMD driver workaround) ──────────────────────
// On AMD Radeon drivers, SDL_CreateWindow(SDL_WINDOW_OPENGL) corrupts the GL
// FBO state for the entire process — any subsequent OGRE FBO creation fails with
// "Framebuffer incomplete".  We bypass SDL entirely for output windows, using
// native Win32 CreateWindowEx + manual pixel format setup + wglMakeCurrent.
// Textures (including the OGRE RenderTexture) are shared via the GL context;
// only the drawable DC changes.
#ifdef _WIN32

bool OutputManager::sWndClassRegistered = false;

static LRESULT CALLBACK bbfxOutputWndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_CLOSE:
        ShowWindow(hwnd, SW_HIDE);
        return 0; // Don't destroy — managed by OutputManager
    case WM_ERASEBKGND:
        return 1; // Suppress background erase — GL handles rendering
    case WM_PAINT: {
        PAINTSTRUCT ps;
        BeginPaint(hwnd, &ps);
        EndPaint(hwnd, &ps);
        return 0; // Validate region without GDI painting — GL handles rendering
    }
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

bool OutputManager::createNativeOutputWindow(OutputSlot& slot) {
    // Register window class (once).
    if (!sWndClassRegistered) {
        WNDCLASSEXW wc = {};
        wc.cbSize        = sizeof(wc);
        wc.style         = CS_OWNDC; // Critical: own DC so SetPixelFormat persists
        wc.lpfnWndProc   = bbfxOutputWndProc;
        wc.hInstance      = GetModuleHandleW(nullptr);
        wc.hCursor        = LoadCursor(nullptr, IDC_ARROW);
        wc.hbrBackground  = static_cast<HBRUSH>(GetStockObject(BLACK_BRUSH));
        wc.lpszClassName  = L"BBFxOutput";
        if (!RegisterClassExW(&wc)) {
            std::cerr << "[OutputManager] RegisterClassExW failed: " << GetLastError() << std::endl;
            return false;
        }
        sWndClassRegistered = true;
    }

    // Create the window.
    HWND hwnd = CreateWindowExW(
        WS_EX_APPWINDOW,
        L"BBFxOutput",
        L"BBFx Output",
        WS_POPUP | WS_VISIBLE,   // Borderless
        CW_USEDEFAULT, CW_USEDEFAULT,
        static_cast<int>(slot.width), static_cast<int>(slot.height),
        nullptr, nullptr,
        GetModuleHandleW(nullptr), nullptr);
    if (!hwnd) {
        std::cerr << "[OutputManager] CreateWindowExW failed: " << GetLastError() << std::endl;
        return false;
    }

    HDC hdc = GetDC(hwnd);
    if (!hdc) {
        DestroyWindow(hwnd);
        return false;
    }

    // Set a pixel format compatible with OpenGL 3.3 core rendering.
    // Use the same pixel format as the main SDL window's DC.
    HWND mainHwnd = nullptr;
    {
        auto props = SDL_GetWindowProperties(mMainWindow);
        mainHwnd = static_cast<HWND>(SDL_GetPointerProperty(props,
            SDL_PROP_WINDOW_WIN32_HWND_POINTER, nullptr));
    }
    if (mainHwnd) {
        HDC mainDC = GetDC(mainHwnd);
        int pf = GetPixelFormat(mainDC);
        PIXELFORMATDESCRIPTOR pfd = {};
        DescribePixelFormat(mainDC, pf, sizeof(pfd), &pfd);
        ReleaseDC(mainHwnd, mainDC);
        if (!SetPixelFormat(hdc, pf, &pfd)) {
            std::cerr << "[OutputManager] SetPixelFormat failed: " << GetLastError() << std::endl;
            ReleaseDC(hwnd, hdc);
            DestroyWindow(hwnd);
            return false;
        }
    } else {
        // Fallback: basic pixel format.
        PIXELFORMATDESCRIPTOR pfd = {};
        pfd.nSize        = sizeof(pfd);
        pfd.nVersion     = 1;
        pfd.dwFlags      = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
        pfd.iPixelType   = PFD_TYPE_RGBA;
        pfd.cColorBits   = 32;
        pfd.cDepthBits   = 24;
        pfd.cStencilBits = 8;
        int pf = ChoosePixelFormat(hdc, &pfd);
        SetPixelFormat(hdc, pf, &pfd);
    }

    slot.nativeHWND = hwnd;
    slot.nativeDC   = hdc;

    std::cout << "[OutputManager] Native output window created for slot " << slot.id
              << " (" << slot.width << "x" << slot.height << ")" << std::endl;
    return true;
}

void OutputManager::destroyNativeOutputWindow(OutputSlot& slot) {
    if (slot.nativeDC && slot.nativeHWND) {
        ReleaseDC(static_cast<HWND>(slot.nativeHWND), static_cast<HDC>(slot.nativeDC));
    }
    if (slot.nativeHWND) {
        DestroyWindow(static_cast<HWND>(slot.nativeHWND));
    }
    slot.nativeHWND = nullptr;
    slot.nativeDC   = nullptr;
}

bool OutputManager::makeNativeWindowCurrent(OutputSlot& slot) {
    if (!slot.nativeDC) return false;
    // SDL3 SDL_GLContext is internally HGLRC on the WGL backend.
    HGLRC hglrc = reinterpret_cast<HGLRC>(mSharedGLContext);
    BOOL ok = wglMakeCurrent(static_cast<HDC>(slot.nativeDC), hglrc);
    return ok != 0;
}

void OutputManager::swapNativeWindow(OutputSlot& slot) {
    if (!slot.nativeDC) return;
    SwapBuffers(static_cast<HDC>(slot.nativeDC));
}

void OutputManager::restoreMainContext() {
    // Use raw wglMakeCurrent with the cached main window DC.
    // SDL_GL_MakeCurrent may not actually call wglMakeCurrent when it detects the
    // same HGLRC is already current (but on a different DC from our native windows).
    if (mMainDC) {
        HGLRC hglrc = reinterpret_cast<HGLRC>(mSharedGLContext);
        wglMakeCurrent(static_cast<HDC>(mMainDC), hglrc);
    }
    // Also notify SDL so its internal tracking stays in sync.
    SDL_GL_MakeCurrent(mMainWindow, mSharedGLContext);
}

#endif // _WIN32

// ── GL3.3 function pointers (cross-platform via SDL_GL_GetProcAddress) ────────
// On Windows, GL3.3 functions are not exported by opengl32.dll, so we load them
// dynamically.  On Linux/macOS the same approach works via SDL_GL_GetProcAddress
// (which wraps glXGetProcAddress / NSGLGetProcAddress).  Using a calling-convention
// macro keeps the typedefs correct on both platforms.
namespace {

#ifdef _WIN32
#  define BBFX_GLCALL APIENTRY
#else
#  define BBFX_GLCALL
#endif

using PFN_glBindFramebuffer     = void (BBFX_GLCALL*)(unsigned int, unsigned int);
using PFN_glCreateShader        = unsigned int (BBFX_GLCALL*)(unsigned int);
using PFN_glShaderSource        = void (BBFX_GLCALL*)(unsigned int, int, const char* const*, const int*);
using PFN_glCompileShader       = void (BBFX_GLCALL*)(unsigned int);
using PFN_glCreateProgram       = unsigned int (BBFX_GLCALL*)();
using PFN_glAttachShader        = void (BBFX_GLCALL*)(unsigned int, unsigned int);
using PFN_glLinkProgram         = void (BBFX_GLCALL*)(unsigned int);
using PFN_glUseProgram          = void (BBFX_GLCALL*)(unsigned int);
using PFN_glGenVertexArrays     = void (BBFX_GLCALL*)(int, unsigned int*);
using PFN_glBindVertexArray     = void (BBFX_GLCALL*)(unsigned int);
using PFN_glGenBuffers          = void (BBFX_GLCALL*)(int, unsigned int*);
using PFN_glBindBuffer          = void (BBFX_GLCALL*)(unsigned int, unsigned int);
using PFN_glBufferData          = void (BBFX_GLCALL*)(unsigned int, ptrdiff_t, const void*, unsigned int);
using PFN_glEnableVertexAttribArray = void (BBFX_GLCALL*)(unsigned int);
using PFN_glVertexAttribPointer = void (BBFX_GLCALL*)(unsigned int, int, unsigned int, unsigned char, int, const void*);
using PFN_glGetUniformLocation  = int  (BBFX_GLCALL*)(unsigned int, const char*);
using PFN_glUniform1i           = void (BBFX_GLCALL*)(int, int);
using PFN_glUniform1f           = void (BBFX_GLCALL*)(int, float);
using PFN_glUniform2f           = void (BBFX_GLCALL*)(int, float, float);
using PFN_glUniform2fv          = void (BBFX_GLCALL*)(int, int, const float*);
using PFN_glUniform4f           = void (BBFX_GLCALL*)(int, float, float, float, float);
using PFN_glUniform4fv          = void (BBFX_GLCALL*)(int, int, const float*);
using PFN_glGetShaderiv         = void (BBFX_GLCALL*)(unsigned int, unsigned int, int*);
using PFN_glGetShaderInfoLog    = void (BBFX_GLCALL*)(unsigned int, int, int*, char*);
using PFN_glGetProgramiv        = void (BBFX_GLCALL*)(unsigned int, unsigned int, int*);
using PFN_glGetProgramInfoLog   = void (BBFX_GLCALL*)(unsigned int, int, int*, char*);
using PFN_glDeleteShader        = void (BBFX_GLCALL*)(unsigned int);
using PFN_glDeleteProgram       = void (BBFX_GLCALL*)(unsigned int);
using PFN_glDeleteVertexArrays  = void (BBFX_GLCALL*)(int, const unsigned int*);
using PFN_glDeleteBuffers       = void (BBFX_GLCALL*)(int, const unsigned int*);

static PFN_glBindFramebuffer        sBindFBO           = nullptr;
static PFN_glCreateShader           sCreateShader      = nullptr;
static PFN_glShaderSource           sShaderSource      = nullptr;
static PFN_glCompileShader          sCompileShader     = nullptr;
static PFN_glCreateProgram          sCreateProgram     = nullptr;
static PFN_glAttachShader           sAttachShader      = nullptr;
static PFN_glLinkProgram            sLinkProgram       = nullptr;
static PFN_glUseProgram             sUseProgram        = nullptr;
static PFN_glGenVertexArrays        sGenVertexArrays   = nullptr;
static PFN_glBindVertexArray        sBindVertexArray   = nullptr;
static PFN_glGenBuffers             sGenBuffers        = nullptr;
static PFN_glBindBuffer             sBindBuffer        = nullptr;
static PFN_glBufferData             sBufferData        = nullptr;
static PFN_glEnableVertexAttribArray sEnableVAA        = nullptr;
static PFN_glVertexAttribPointer    sVertexAttribPtr   = nullptr;
static PFN_glGetUniformLocation     sGetUniformLoc     = nullptr;
static PFN_glUniform1i              sUniform1i         = nullptr;
static PFN_glUniform1f              sUniform1f         = nullptr;
static PFN_glUniform2f              sUniform2f         = nullptr;
static PFN_glUniform2fv             sUniform2fv        = nullptr;
static PFN_glUniform4f              sUniform4f         = nullptr;
static PFN_glUniform4fv             sUniform4fv        = nullptr;
static PFN_glGetShaderiv            sGetShaderiv       = nullptr;
static PFN_glGetShaderInfoLog       sGetShaderInfoLog  = nullptr;
static PFN_glGetProgramiv           sGetProgramiv      = nullptr;
static PFN_glGetProgramInfoLog      sGetProgramInfoLog = nullptr;
static PFN_glDeleteShader           sDeleteShader      = nullptr;
static PFN_glDeleteProgram          sDeleteProgram     = nullptr;
static PFN_glDeleteVertexArrays     sDeleteVertexArrays = nullptr;
static PFN_glDeleteBuffers          sDeleteBuffers     = nullptr;

static void ensureBindFBO() {
    if (!sBindFBO)
        sBindFBO = reinterpret_cast<PFN_glBindFramebuffer>(SDL_GL_GetProcAddress("glBindFramebuffer"));
}

static void ensureGL3Funcs() {
    auto get = [](const char* name) { return SDL_GL_GetProcAddress(name); };
    if (!sCreateShader) {
        sCreateShader     = reinterpret_cast<PFN_glCreateShader>(get("glCreateShader"));
        sShaderSource     = reinterpret_cast<PFN_glShaderSource>(get("glShaderSource"));
        sCompileShader    = reinterpret_cast<PFN_glCompileShader>(get("glCompileShader"));
        sCreateProgram    = reinterpret_cast<PFN_glCreateProgram>(get("glCreateProgram"));
        sAttachShader     = reinterpret_cast<PFN_glAttachShader>(get("glAttachShader"));
        sLinkProgram      = reinterpret_cast<PFN_glLinkProgram>(get("glLinkProgram"));
        sUseProgram       = reinterpret_cast<PFN_glUseProgram>(get("glUseProgram"));
        sGenVertexArrays  = reinterpret_cast<PFN_glGenVertexArrays>(get("glGenVertexArrays"));
        sBindVertexArray  = reinterpret_cast<PFN_glBindVertexArray>(get("glBindVertexArray"));
        sGenBuffers       = reinterpret_cast<PFN_glGenBuffers>(get("glGenBuffers"));
        sBindBuffer       = reinterpret_cast<PFN_glBindBuffer>(get("glBindBuffer"));
        sBufferData       = reinterpret_cast<PFN_glBufferData>(get("glBufferData"));
        sEnableVAA        = reinterpret_cast<PFN_glEnableVertexAttribArray>(get("glEnableVertexAttribArray"));
        sVertexAttribPtr  = reinterpret_cast<PFN_glVertexAttribPointer>(get("glVertexAttribPointer"));
        sGetUniformLoc    = reinterpret_cast<PFN_glGetUniformLocation>(get("glGetUniformLocation"));
        sUniform1i        = reinterpret_cast<PFN_glUniform1i>(get("glUniform1i"));
        sUniform1f        = reinterpret_cast<PFN_glUniform1f>(get("glUniform1f"));
        sUniform2f        = reinterpret_cast<PFN_glUniform2f>(get("glUniform2f"));
        sUniform2fv       = reinterpret_cast<PFN_glUniform2fv>(get("glUniform2fv"));
        sUniform4f        = reinterpret_cast<PFN_glUniform4f>(get("glUniform4f"));
        sUniform4fv       = reinterpret_cast<PFN_glUniform4fv>(get("glUniform4fv"));
        sGetShaderiv      = reinterpret_cast<PFN_glGetShaderiv>(get("glGetShaderiv"));
        sGetShaderInfoLog = reinterpret_cast<PFN_glGetShaderInfoLog>(get("glGetShaderInfoLog"));
        sGetProgramiv     = reinterpret_cast<PFN_glGetProgramiv>(get("glGetProgramiv"));
        sGetProgramInfoLog = reinterpret_cast<PFN_glGetProgramInfoLog>(get("glGetProgramInfoLog"));
        sDeleteShader     = reinterpret_cast<PFN_glDeleteShader>(get("glDeleteShader"));
        sDeleteProgram    = reinterpret_cast<PFN_glDeleteProgram>(get("glDeleteProgram"));
        sDeleteVertexArrays = reinterpret_cast<PFN_glDeleteVertexArrays>(get("glDeleteVertexArrays"));
        sDeleteBuffers    = reinterpret_cast<PFN_glDeleteBuffers>(get("glDeleteBuffers"));
    }
}

/// Check shader compilation status and log errors.
static bool checkShaderCompile(unsigned int shader, const char* label) {
    if (!sGetShaderiv || !sGetShaderInfoLog) return true; // can't check
    int status = 0;
    sGetShaderiv(shader, 0x8B81 /*GL_COMPILE_STATUS*/, &status);
    if (status) return true;
    char buf[2048] = {};
    int len = 0;
    sGetShaderInfoLog(shader, sizeof(buf) - 1, &len, buf);
    std::cerr << "[OutputManager] " << label << " compile FAILED:\n" << buf << std::endl;
    return false;
}

/// Check program link status and log errors.
static bool checkProgramLink(unsigned int prog) {
    if (!sGetProgramiv || !sGetProgramInfoLog) return true; // can't check
    int status = 0;
    sGetProgramiv(prog, 0x8B82 /*GL_LINK_STATUS*/, &status);
    if (status) return true;
    char buf[2048] = {};
    int len = 0;
    sGetProgramInfoLog(prog, sizeof(buf) - 1, &len, buf);
    std::cerr << "[OutputManager] Program link FAILED:\n" << buf << std::endl;
    return false;
}

} // anon

// ── OutputSlot JSON ─────────────────────────────────���───────────────────────

nlohmann::json OutputSlot::toJson() const {
    nlohmann::json j = {
        {"id", id},
        {"width", width},
        {"height", height},
        {"monitorIndex", monitorIndex},
        {"fullscreen", fullscreen},
        {"warpEnabled", warpEnabled},
        {"warp", warpProfile.toJson()},
        {"blendEnabled", blendEnabled},
        {"blend", blendProfile.toJson()},
        {"textureShareEnabled", textureShareEnabled},
        {"textureShareSourceName", textureShareSourceName},
        {"spoutEnabled", textureShareEnabled},
        {"spoutSourceName", textureShareSourceName},
        {"gridWarpEnabled", gridWarpEnabled},
        {"gridWarp", gridWarpProfile.toJson()},
        {"zoneId", zoneId},
        {"visible", visible}
    };
    return j;
}

void OutputSlot::fromJson(const nlohmann::json& j) {
    if (j.contains("id"))             id = j["id"].get<int>();
    if (j.contains("width"))          width = j["width"].get<uint32_t>();
    if (j.contains("height"))         height = j["height"].get<uint32_t>();
    if (j.contains("monitorIndex"))   monitorIndex = j["monitorIndex"].get<int>();
    if (j.contains("fullscreen"))     fullscreen = j["fullscreen"].get<bool>();
    if (j.contains("warpEnabled"))    warpEnabled = j["warpEnabled"].get<bool>();
    if (j.contains("warp"))           warpProfile.fromJson(j["warp"]);
    if (j.contains("blendEnabled"))   blendEnabled = j["blendEnabled"].get<bool>();
    if (j.contains("blend"))          blendProfile.fromJson(j["blend"]);
    // Texture sharing: read new keys first, fallback to legacy Spout keys for v3.4.0 compat.
    if (j.contains("textureShareEnabled"))    textureShareEnabled = j["textureShareEnabled"].get<bool>();
    else if (j.contains("spoutEnabled"))      textureShareEnabled = j["spoutEnabled"].get<bool>();
    if (j.contains("textureShareSourceName")) textureShareSourceName = j["textureShareSourceName"].get<std::string>();
    else if (j.contains("spoutSourceName"))   textureShareSourceName = j["spoutSourceName"].get<std::string>();
    if (j.contains("gridWarpEnabled")) gridWarpEnabled = j["gridWarpEnabled"].get<bool>();
    if (j.contains("gridWarp"))       gridWarpProfile.fromJson(j["gridWarp"]);
    if (j.contains("zoneId"))         zoneId = j["zoneId"].get<int>();
    if (j.contains("visible"))        visible = j["visible"].get<bool>();
}

// ── OutputManager ───────────────────────────────────────────────────────────

OutputManager::OutputManager(SDL_GLContext sharedContext, SDL_Window* mainWindow)
    : mSharedGLContext(sharedContext)
    , mMainWindow(mainWindow)
{
#ifdef _WIN32
    // Cache the main window's HDC for wglMakeCurrent context restore.
    // SDL_GL_MakeCurrent may not reliably switch back when using the same HGLRC
    // across multiple DCs (AMD driver issue).
    HWND hwnd = static_cast<HWND>(SDL_GetPointerProperty(
        SDL_GetWindowProperties(mainWindow), SDL_PROP_WINDOW_WIN32_HWND_POINTER, nullptr));
    if (hwnd)
        mMainDC = static_cast<void*>(GetDC(hwnd));
#endif
}

OutputManager::~OutputManager() {
    // Close all output windows.
    while (!mSlots.empty()) {
        removeOutput(mSlots.back().id);
    }
    // Cleanup blit GL resources.
    if (mBlitProg && sDeleteProgram)       sDeleteProgram(mBlitProg);
    if (mBlitVAO  && sDeleteVertexArrays)  sDeleteVertexArrays(1, &mBlitVAO);
    // Cleanup test pattern GL resources (Lot D).
    if (mTestPatternProg && sDeleteProgram)       sDeleteProgram(mTestPatternProg);
    if (mTestPatternVBO  && sDeleteBuffers)       sDeleteBuffers(1, &mTestPatternVBO);
    if (mTestPatternVAO  && sDeleteVertexArrays)  sDeleteVertexArrays(1, &mTestPatternVAO);
}

int OutputManager::addOutput(int width, int height, Ogre::SceneManager* sceneMgr) {
    OutputSlot slot;
    slot.id = mNextId++;
    slot.width = static_cast<uint32_t>(width);
    slot.height = static_cast<uint32_t>(height);

    // DEFERRED WINDOW CREATION: Don't create the SDL window during addOutput.
    // On AMD drivers, SDL_CreateWindow(SDL_WINDOW_OPENGL) corrupts the GL FBO
    // state even for the main window, breaking OGRE's render textures.
    // The window will be created lazily on first updateAll() call.
    slot.window = nullptr; // Created later in updateAll()

    std::cout << "[OutputManager] Output " << slot.id << " created ("
              << width << "x" << height << ")" << std::endl;

    mSlots.push_back(std::move(slot));
    return mSlots.back().id;
}

void OutputManager::removeOutput(int id) {
    for (auto it = mSlots.begin(); it != mSlots.end(); ++it) {
        if (it->id == id) {
            // Release texture share sender if active.
            if (it->textureSender) {
                it->textureSender->release();
                it->textureSender.reset();
            }
#ifdef _WIN32
            destroyNativeOutputWindow(*it);
#else
            if (it->window) {
                SDL_DestroyWindow(it->window);
                it->window = nullptr;
            }
#endif
            std::cout << "[OutputManager] Output " << id << " removed" << std::endl;
            mSlots.erase(it);
            return;
        }
    }
}

OutputSlot* OutputManager::getSlot(int id) {
    for (auto& s : mSlots) {
        if (s.id == id) return &s;
    }
    return nullptr;
}

const OutputSlot* OutputManager::getSlot(int id) const {
    for (const auto& s : mSlots) {
        if (s.id == id) return &s;
    }
    return nullptr;
}

void OutputManager::updateAll() {
    if (mSlots.empty()) return;

    ensureBindFBO();

    // Get the GL texture ID from the main scene RenderTexture (set via setSourceTexture).
    unsigned int srcTexId = 0;
    if (mSourceTexture)
        mSourceTexture->getCustomAttribute("GLID", &srcTexId);

    // Texture sharing (v3.4 Lot H): send main texture for each slot that needs it.
    if (srcTexId) {
        for (auto& slot : mSlots) {
            if (!slot.visible) continue; // Skip hidden outputs
            if (!slot.textureShareEnabled || !slot.textureSender) continue;
            if (!slot.textureSender->isInitialised()) {
                std::string name = slot.textureShareSourceName.empty()
                    ? ("BBFx Output " + std::to_string(slot.id)) : slot.textureShareSourceName;
                slot.textureSender->setName(name);
                slot.textureSender->init(static_cast<int>(slot.width), static_cast<int>(slot.height));
            }
            slot.textureSender->sendTexture(srcTexId,
                static_cast<int>(slot.width), static_cast<int>(slot.height));
        }
    }

    // Blit the main scene texture to each output window.
    for (auto& slot : mSlots) {
#ifdef _WIN32
        // Lazy native window creation (AMD driver workaround).
        if (!slot.nativeHWND) {
            if (!createNativeOutputWindow(slot))
                continue;
            // If slot was marked hidden before window creation, hide immediately.
            if (!slot.visible)
                ShowWindow(static_cast<HWND>(slot.nativeHWND), SW_HIDE);
            // Apply deferred monitor/fullscreen settings.
            if (slot.monitorIndex >= 0)
                setMonitor(slot.id, slot.monitorIndex);
            if (slot.fullscreen) {
                // Fullscreen via Win32: remove borders and cover the monitor.
                int displayCount = 0;
                auto* displays = SDL_GetDisplays(&displayCount);
                int monIdx = slot.monitorIndex >= 0 ? slot.monitorIndex : 0;
                if (displays && monIdx < displayCount) {
                    SDL_Rect bounds;
                    SDL_GetDisplayBounds(displays[monIdx], &bounds);
                    SetWindowPos(static_cast<HWND>(slot.nativeHWND), HWND_TOP,
                        bounds.x, bounds.y, bounds.w, bounds.h,
                        slot.visible ? SWP_SHOWWINDOW : SWP_NOACTIVATE);
                }
                if (displays) SDL_free(displays);
            }
        }

        // Skip rendering for hidden outputs (no GL context switch, no blit, no swap).
        if (!slot.visible) continue;

        if (!makeNativeWindowCurrent(slot))
            continue;

        if (slot.testPattern.active) {
            renderTestPattern(slot);
        } else if (srcTexId) {
            blitToWindow(slot, srcTexId);
        }

        swapNativeWindow(slot);
#else
        // Non-Windows: use SDL windows (no AMD FBO bug).
        if (!slot.window) {
            slot.window = SDL_CreateWindow(
                ("BBFx Output " + std::to_string(slot.id)).c_str(),
                static_cast<int>(slot.width), static_cast<int>(slot.height),
                SDL_WINDOW_OPENGL | SDL_WINDOW_BORDERLESS);
            if (!slot.window) continue;
            SDL_GL_MakeCurrent(mMainWindow, mSharedGLContext);
            if (slot.monitorIndex >= 0) setMonitor(slot.id, slot.monitorIndex);
            if (slot.fullscreen) SDL_SetWindowFullscreen(slot.window, true);
            // If slot was marked hidden before window creation, hide immediately.
            if (!slot.visible) SDL_HideWindow(slot.window);
        }
        // Skip rendering for hidden outputs.
        if (!slot.visible) continue;
        SDL_GL_MakeCurrent(slot.window, mSharedGLContext);
        if (slot.testPattern.active) {
            renderTestPattern(slot);
        } else if (srcTexId) {
            blitToWindow(slot, srcTexId);
        }
        SDL_GL_SwapWindow(slot.window);
#endif
    }

    // Restore main window context for ImGui / OGRE main rendering.
#ifdef _WIN32
    restoreMainContext();
#else
    SDL_GL_MakeCurrent(mMainWindow, mSharedGLContext);
#endif
}

void OutputManager::setOutputVisible(int id, bool vis) {
    auto* slot = getSlot(id);
    if (!slot || slot->visible == vis) return;
    slot->visible = vis;
#ifdef _WIN32
    if (slot->nativeHWND) {
        ShowWindow(static_cast<HWND>(slot->nativeHWND), vis ? SW_SHOW : SW_HIDE);
    }
#else
    if (slot->window) {
        if (vis) SDL_ShowWindow(slot->window);
        else     SDL_HideWindow(slot->window);
    }
#endif
    std::cout << "[OutputManager] Output " << id
              << (vis ? " shown" : " hidden") << std::endl;
}

bool OutputManager::isOutputVisible(int id) const {
    const auto* slot = getSlot(id);
    return slot ? slot->visible : false;
}

void OutputManager::enableTextureShare(int id, const std::string& sourceName) {
    auto* slot = getSlot(id);
    if (!slot) return;
    if (!slot->textureSender) slot->textureSender = createTextureSender();
    slot->textureShareEnabled = true;
    if (!sourceName.empty()) slot->textureShareSourceName = sourceName;
    if (slot->textureShareSourceName.empty())
        slot->textureShareSourceName = "BBFx Output " + std::to_string(id);
    std::cout << "[OutputManager] Texture sharing enabled on slot " << id
              << " (\"" << slot->textureShareSourceName << "\", backend: "
              << slot->textureSender->backendName() << ")" << std::endl;
}

void OutputManager::disableTextureShare(int id) {
    auto* slot = getSlot(id);
    if (!slot) return;
    slot->textureShareEnabled = false;
    if (slot->textureSender) slot->textureSender->release();
    std::cout << "[OutputManager] Texture sharing disabled on slot " << id << std::endl;
}

bool OutputManager::isTextureShareEnabled(int id) const {
    const auto* slot = getSlot(id);
    return slot && slot->textureShareEnabled;
}

void OutputManager::setMonitor(int id, int monitorIndex) {
    auto* slot = getSlot(id);
    if (!slot) return;

    int count = 0;
    auto* displays = SDL_GetDisplays(&count);
    if (monitorIndex >= 0 && monitorIndex < count && displays) {
        SDL_Rect bounds;
        SDL_GetDisplayBounds(displays[monitorIndex], &bounds);
#ifdef _WIN32
        if (slot->nativeHWND) {
            SetWindowPos(static_cast<HWND>(slot->nativeHWND), nullptr,
                bounds.x, bounds.y, 0, 0, SWP_NOSIZE | SWP_NOZORDER);
        }
#else
        if (slot->window) {
            SDL_SetWindowPosition(slot->window, bounds.x, bounds.y);
        }
#endif
        slot->monitorIndex = monitorIndex;
    }
    if (displays) SDL_free(displays);
}

void OutputManager::setResolution(int id, int w, int h, Ogre::SceneManager* sceneMgr) {
    auto* slot = getSlot(id);
    if (!slot) return;

    // Close and re-open with new resolution.
    int monIdx = slot->monitorIndex;
    bool fs = slot->fullscreen;
    removeOutput(id);

    // Re-create — note: id will be different after re-creation.
    int newId = addOutput(w, h, sceneMgr);
    if (newId >= 0) {
        auto* newSlot = getSlot(newId);
        if (newSlot) {
            newSlot->monitorIndex = monIdx;
            newSlot->fullscreen = fs;
            if (monIdx >= 0) setMonitor(newId, monIdx);
            if (fs) toggleFullscreen(newId);
        }
    }
}

void OutputManager::toggleFullscreen(int id) {
    auto* slot = getSlot(id);
    if (!slot) return;
    slot->fullscreen = !slot->fullscreen;
#ifdef _WIN32
    if (slot->nativeHWND) {
        HWND hwnd = static_cast<HWND>(slot->nativeHWND);
        if (slot->fullscreen) {
            // Cover the target monitor.
            int displayCount = 0;
            auto* displays = SDL_GetDisplays(&displayCount);
            int monIdx = slot->monitorIndex >= 0 ? slot->monitorIndex : 0;
            if (displays && monIdx < displayCount) {
                SDL_Rect bounds;
                SDL_GetDisplayBounds(displays[monIdx], &bounds);
                // Only show the window if the slot is visible.
                UINT flags = slot->visible ? SWP_SHOWWINDOW : SWP_NOACTIVATE;
                SetWindowPos(hwnd, HWND_TOP,
                    bounds.x, bounds.y, bounds.w, bounds.h, flags);
            }
            if (displays) SDL_free(displays);
        } else {
            // Windowed: restore to slot size.
            SetWindowPos(hwnd, nullptr, CW_USEDEFAULT, CW_USEDEFAULT,
                static_cast<int>(slot->width), static_cast<int>(slot->height),
                SWP_NOZORDER);
        }
    }
#else
    if (slot->window)
        SDL_SetWindowFullscreen(slot->window, slot->fullscreen);
#endif
}

void OutputManager::enableWarp(int id) {
    auto* slot = getSlot(id);
    if (!slot) return;
    slot->warpEnabled = true;
    std::cout << "[OutputManager] Warp enabled on output " << id << std::endl;
}

void OutputManager::disableWarp(int id) {
    auto* slot = getSlot(id);
    if (!slot) return;
    slot->warpEnabled = false;
    std::cout << "[OutputManager] Warp disabled on output " << id << std::endl;
}

void OutputManager::updateWarpParams(int id) {
    auto* slot = getSlot(id);
    if (!slot || !slot->warpEnabled) return;

    // Update the shared BBFx/QuadWarp material uniforms.
    // Rendering is sequential so updating before each render is safe.
    auto mat = Ogre::MaterialManager::getSingleton().getByName("BBFx/QuadWarp");
    if (!mat) return;
    auto* pass = mat->getTechnique(0)->getPass(0);
    auto params = pass->getFragmentProgramParameters();
    if (!params) return;

    const float* c = slot->warpProfile.corners;
    try {
        params->setNamedConstant("tl_x", c[0]);
        params->setNamedConstant("tl_y", c[1]);
        params->setNamedConstant("tr_x", c[2]);
        params->setNamedConstant("tr_y", c[3]);
        params->setNamedConstant("bl_x", c[4]);
        params->setNamedConstant("bl_y", c[5]);
        params->setNamedConstant("br_x", c[6]);
        params->setNamedConstant("br_y", c[7]);
    } catch (const std::exception& e) {
        std::cerr << "[OutputManager] updateWarpParams error: " << e.what() << std::endl;
    }
}

void OutputManager::resetAllWarps() {
    for (auto& slot : mSlots) {
        slot.warpProfile.reset();
        if (slot.warpEnabled) {
            updateWarpParams(slot.id);
        }
    }
    std::cout << "[OutputManager] All warps reset to identity" << std::endl;
}

// ── Grid Warp (v3.4 Lot K) ───────────────────────────────────────────────────

void OutputManager::enableGridWarp(int id) {
    auto* slot = getSlot(id);
    if (!slot) return;
    slot->gridWarpEnabled = true;
    std::cout << "[OutputManager] GridWarp enabled on output " << id << std::endl;
}

void OutputManager::disableGridWarp(int id) {
    auto* slot = getSlot(id);
    if (!slot) return;
    slot->gridWarpEnabled = false;
    std::cout << "[OutputManager] GridWarp disabled on output " << id << std::endl;
}

void OutputManager::updateGridWarpParams(int id) {
    auto* slot = getSlot(id);
    if (!slot || !slot->gridWarpEnabled) return;

    auto mat = Ogre::MaterialManager::getSingleton().getByName("BBFx/GridWarp");
    if (!mat) return;
    auto* pass   = mat->getTechnique(0)->getPass(0);
    auto  params = pass->getFragmentProgramParameters();
    if (!params) return;

    // Pack 16 vec2 control points into 8 vec4 (xy=point_i*2, zw=point_i*2+1)
    // Use raw float array: count=32, multiple=1 → writes 32 floats (8 vec4).
    const auto& gw = slot->gridWarpProfile;
    float flatGrid[32]; // 8 vec4 = 32 floats
    for (int i = 0; i < 8; ++i) {
        int a = i * 2, b = i * 2 + 1;
        flatGrid[i * 4 + 0] = gw.pts[a * 2 + 0]; // xy of point a
        flatGrid[i * 4 + 1] = gw.pts[a * 2 + 1];
        flatGrid[i * 4 + 2] = gw.pts[b * 2 + 0]; // xy of point b
        flatGrid[i * 4 + 3] = gw.pts[b * 2 + 1];
    }
    try {
        params->setNamedConstant("grid", flatGrid, 32, 1);
    } catch (const std::exception& e) {
        std::cerr << "[OutputManager] updateGridWarpParams error: " << e.what() << std::endl;
    }
}

void OutputManager::resetAllGridWarps() {
    for (auto& slot : mSlots) {
        slot.gridWarpProfile.reset();
        if (slot.gridWarpEnabled) {
            updateGridWarpParams(slot.id);
        }
    }
    std::cout << "[OutputManager] All grid warps reset to identity" << std::endl;
}

void OutputManager::enableBlend(int id) {
    auto* slot = getSlot(id);
    if (!slot) return;
    slot->blendEnabled = true;
    std::cout << "[OutputManager] Blend enabled on output " << id << std::endl;
}

void OutputManager::disableBlend(int id) {
    auto* slot = getSlot(id);
    if (!slot) return;
    slot->blendEnabled = false;
    std::cout << "[OutputManager] Blend disabled on output " << id << std::endl;
}

void OutputManager::updateBlendParams(int id) {
    auto* slot = getSlot(id);
    if (!slot || !slot->blendEnabled) return;

    auto mat = Ogre::MaterialManager::getSingleton().getByName("BBFx/EdgeBlend");
    if (!mat) return;
    auto* pass = mat->getTechnique(0)->getPass(0);
    auto params = pass->getFragmentProgramParameters();
    if (!params) return;

    try {
        params->setNamedConstant("blend_left",   slot->blendProfile.left);
        params->setNamedConstant("blend_right",  slot->blendProfile.right);
        params->setNamedConstant("blend_top",    slot->blendProfile.top);
        params->setNamedConstant("blend_bottom", slot->blendProfile.bottom);
        params->setNamedConstant("blend_gamma",  slot->blendProfile.gamma);
    } catch (const std::exception& e) {
        std::cerr << "[OutputManager] updateBlendParams error: " << e.what() << std::endl;
    }
}

void OutputManager::resetAllBlends() {
    for (auto& slot : mSlots) {
        slot.blendProfile.reset();
        if (slot.blendEnabled) {
            updateBlendParams(slot.id);
        }
    }
    std::cout << "[OutputManager] All blends reset to default" << std::endl;
}

void OutputManager::applyZoneWarpBlend(int slotId, const WarpProfile& wp, bool warpEn,
                                       const BlendProfile& bp, bool blendEn) {
    auto* slot = getSlot(slotId);
    if (!slot) {
        std::cerr << "[OutputManager] applyZoneWarpBlend: slot " << slotId << " not found" << std::endl;
        return;
    }
    slot->warpProfile  = wp;
    slot->warpEnabled  = warpEn;
    slot->blendProfile = bp;
    slot->blendEnabled = blendEn;
    if (warpEn)  { enableWarp(slotId);  updateWarpParams(slotId); }
    else         { disableWarp(slotId); }
    if (blendEn) { enableBlend(slotId); updateBlendParams(slotId); }
    else         { disableBlend(slotId); }
}

std::vector<OutputManager::BlendSuggestion> OutputManager::detectAdjacentOutputs() const {
    std::vector<BlendSuggestion> suggestions;
    int displayCount = 0;
    auto* displays = SDL_GetDisplays(&displayCount);
    if (!displays) return suggestions;

    // Collect monitor bounds for each slot
    struct SlotInfo { int id; SDL_Rect bounds; };
    std::vector<SlotInfo> infos;
    for (const auto& slot : mSlots) {
        if (slot.monitorIndex >= 0 && slot.monitorIndex < displayCount) {
            SDL_Rect r;
            SDL_GetDisplayBounds(displays[slot.monitorIndex], &r);
            infos.push_back({slot.id, r});
        }
    }
    SDL_free(displays);

    for (size_t i = 0; i < infos.size(); ++i) {
        for (size_t k = i + 1; k < infos.size(); ++k) {
            const auto& a = infos[i].bounds;
            const auto& b = infos[k].bounds;
            // Horizontal adjacency: b immediately to the right of a
            if (a.x + a.w == b.x || b.x + b.w == a.x) {
                suggestions.push_back({infos[i].id, infos[k].id, true, 0.15f});
            }
            // Vertical adjacency: b immediately below a
            if (a.y + a.h == b.y || b.y + b.h == a.y) {
                suggestions.push_back({infos[i].id, infos[k].id, false, 0.15f});
            }
        }
    }
    return suggestions;
}

ImTextureID OutputManager::getTextureID(int id) const {
    // All outputs share the same source texture (main scene RT).
    if (!mSourceTexture) return 0;
    unsigned int glId = 0;
    mSourceTexture->getCustomAttribute("GLID", &glId);
    return static_cast<ImTextureID>(glId);
}

nlohmann::json OutputManager::toJson() const {
    nlohmann::json arr = nlohmann::json::array();
    for (const auto& slot : mSlots) {
        arr.push_back(slot.toJson());
    }
    return arr;
}

void OutputManager::fromJson(const nlohmann::json& j, Ogre::SceneManager* sceneMgr) {
    // Close existing outputs.
    while (!mSlots.empty()) {
        removeOutput(mSlots.back().id);
    }

    if (!j.is_array()) return;

    for (const auto& slotJson : j) {
        int w = slotJson.value("width", 1920);
        int h = slotJson.value("height", 1080);
        int newId = addOutput(w, h, sceneMgr);
        if (newId >= 0) {
            auto* slot = getSlot(newId);
            if (slot) {
                bool wasWarpEnabled     = slotJson.value("warpEnabled",     false);
                bool wasBlendEnabled    = slotJson.value("blendEnabled",    false);
                bool wasGridWarpEnabled = slotJson.value("gridWarpEnabled", false);
                bool wasTexShareEnabled = slotJson.value("textureShareEnabled",
                                           slotJson.value("spoutEnabled", false));
                slot->fromJson(slotJson);
                if (slot->monitorIndex >= 0) setMonitor(newId, slot->monitorIndex);
                if (slot->fullscreen) toggleFullscreen(newId);
                if (wasBlendEnabled)    enableBlend(newId);     // blend before warp
                if (wasWarpEnabled)     enableWarp(newId);
                if (wasGridWarpEnabled) enableGridWarp(newId);  // grid warp after quad warp
                if (wasTexShareEnabled) enableTextureShare(newId, slot->textureShareSourceName);
                // Visibility defaults to true; if saved as hidden, apply it.
                // Window doesn't exist yet (lazy creation), so the flag is enough —
                // updateAll() will hide the window immediately after creation.
            }
        }
    }
}

// ── WarpWizard test pattern GL rendering (v3.4 Lot D) ────────────────────────

/// Blit shader: fullscreen quad that samples from an OGRE RenderTexture.
static const char* kBlitVert = R"GLSL(
#version 330 core
out vec2 vUV;
void main() {
    // Generate fullscreen triangle from gl_VertexID — no VBO needed.
    // Vertices 0,1,2 → oversized triangle covering [-1,1]x[-1,1].
    vec2 pos = vec2(
        float((gl_VertexID & 1) * 4 - 1),
        float((gl_VertexID & 2) * 2 - 1)
    );
    gl_Position = vec4(pos, 0.0, 1.0);
    vUV = pos * 0.5 + 0.5;
}
)GLSL";

static const char* kBlitFrag = R"GLSL(
#version 330 core
in vec2 vUV;
out vec4 fragColor;

uniform sampler2D uTex;

// Zone crop: normalised sub-region (x, y, width, height) of the source texture.
// Default (0,0,1,1) = full texture (no crop).
uniform vec4 uZoneCrop;

// Quad warp (inverse bilinear — 4 corner offsets)
uniform int  uWarpEnabled;
uniform vec2 uWarpTL;
uniform vec2 uWarpTR;
uniform vec2 uWarpBL;
uniform vec2 uWarpBR;

// Grid warp (4x4 bilinear — 16 vec2 packed as 8 vec4)
uniform int  uGridWarpEnabled;
uniform vec4 uGrid[8];

// Edge blend
uniform int   uBlendEnabled;
uniform float uBlendLeft;
uniform float uBlendRight;
uniform float uBlendTop;
uniform float uBlendBottom;
uniform float uBlendGamma;

// ── Inverse bilinear (Inigo Quilez) ─────────────────────────────────────────
float cross2(vec2 a, vec2 b) { return a.x * b.y - a.y * b.x; }

vec2 invBilinear(vec2 p, vec2 tl, vec2 tr, vec2 bl, vec2 br) {
    vec2 e = tr - tl;
    vec2 f = bl - tl;
    vec2 g = tl - tr - bl + br;
    vec2 h = p - tl;

    float k2 = cross2(g, f);
    float k1 = cross2(e, f) + cross2(h, g);
    float k0 = cross2(h, e);

    float s, t;

    if (abs(k2) < 1e-6) {
        float denom = k1 - k0;
        t = (abs(denom) > 1e-9) ? -k0 / denom : 0.0;
        float ex = e.x + g.x * t;
        float ey = e.y + g.y * t;
        s = (abs(ex) > abs(ey)) ? (h.x - f.x * t) / (ex + 1e-12)
                                 : (h.y - f.y * t) / (ey + 1e-12);
    } else {
        float disc = k1 * k1 - 4.0 * k0 * k2;
        if (disc < 0.0) return vec2(-1.0);
        float w   = sqrt(disc);
        float ik2 = 0.5 / k2;

        t = (-k1 - w) * ik2;
        float ex = e.x + g.x * t, ey = e.y + g.y * t;
        s = (abs(ex) > abs(ey)) ? (h.x - f.x * t) / (ex + 1e-12)
                                 : (h.y - f.y * t) / (ey + 1e-12);

        if (s < -1e-4 || s > 1.0 + 1e-4 || t < -1e-4 || t > 1.0 + 1e-4) {
            t  = (-k1 + w) * ik2;
            ex = e.x + g.x * t; ey = e.y + g.y * t;
            s  = (abs(ex) > abs(ey)) ? (h.x - f.x * t) / (ex + 1e-12)
                                      : (h.y - f.y * t) / (ey + 1e-12);
        }
    }
    return vec2(s, t);
}

// ── Grid warp (4x4 bilinear mesh) ───────────────────────────────────────────
vec2 getGridPoint(int idx) {
    int i   = idx / 2;
    bool lo = (idx % 2 == 0);
    return lo ? uGrid[i].xy : uGrid[i].zw;
}

vec2 gridWarp(vec2 p) {
    const int N = 4;
    float invN1 = 1.0 / float(N - 1);
    int ci = clamp(int(p.x / invN1), 0, N - 2);
    int cj = clamp(int(p.y / invN1), 0, N - 2);
    float s = clamp((p.x - float(ci) * invN1) / invN1, 0.0, 1.0);
    float t = clamp((p.y - float(cj) * invN1) / invN1, 0.0, 1.0);
    vec2 tl = getGridPoint( cj      * N + ci);
    vec2 tr = getGridPoint( cj      * N + ci + 1);
    vec2 bl = getGridPoint((cj + 1) * N + ci);
    vec2 br = getGridPoint((cj + 1) * N + ci + 1);
    return mix(mix(tl, tr, s), mix(bl, br, s), t);
}

// ── Main ─────────────────────────────────────────────────────────────────────
void main() {
    vec2 uv = vUV;

    // [1] Warp: transform screen UV to source UV
    if (uWarpEnabled != 0) {
        vec2 st = invBilinear(vUV, uWarpTL, uWarpTR, uWarpBL, uWarpBR);
        if (st.x >= -1e-4 && st.x <= 1.0 + 1e-4 && st.y >= -1e-4 && st.y <= 1.0 + 1e-4) {
            uv = clamp(st, 0.0, 1.0);
        } else {
            fragColor = vec4(0.0, 0.0, 0.0, 1.0);
            return;
        }
    } else if (uGridWarpEnabled != 0) {
        uv = clamp(gridWarp(vUV), 0.0, 1.0);
    }

    // [2] Zone crop: map [0,1] warped UV to zone sub-region of source texture
    uv = uZoneCrop.xy + uv * uZoneCrop.zw;

    // [2b] Flip Y: OGRE renders with origin top-left, GL texture has origin bottom-left
    uv.y = 1.0 - uv.y;

    // [3] Sample
    fragColor = texture(uTex, uv);

    // [4] Edge blend (on screen-space coords, not warped UV)
    if (uBlendEnabled != 0) {
        float u = vUV.x;
        float v = vUV.y;
        float factor = 1.0;
        if (uBlendLeft > 1e-5 && u < uBlendLeft)
            factor *= pow(clamp(u / uBlendLeft, 0.0, 1.0), uBlendGamma);
        if (uBlendRight > 1e-5 && u > 1.0 - uBlendRight)
            factor *= pow(clamp((1.0 - u) / uBlendRight, 0.0, 1.0), uBlendGamma);
        if (uBlendTop > 1e-5 && v < uBlendTop)
            factor *= pow(clamp(v / uBlendTop, 0.0, 1.0), uBlendGamma);
        if (uBlendBottom > 1e-5 && v > 1.0 - uBlendBottom)
            factor *= pow(clamp((1.0 - v) / uBlendBottom, 0.0, 1.0), uBlendGamma);
        fragColor.rgb *= factor;
    }
}
)GLSL";

void OutputManager::initBlitGL() {
    ensureGL3Funcs();
    if (!sCreateShader || !sCreateProgram) return;

    unsigned int vs = sCreateShader(0x8B31 /*GL_VERTEX_SHADER*/);
    sShaderSource(vs, 1, &kBlitVert, nullptr);
    sCompileShader(vs);
    if (!checkShaderCompile(vs, "blit vertex")) {
        if (sDeleteShader) sDeleteShader(vs);
        return;
    }

    unsigned int fs = sCreateShader(0x8B30 /*GL_FRAGMENT_SHADER*/);
    sShaderSource(fs, 1, &kBlitFrag, nullptr);
    sCompileShader(fs);
    if (!checkShaderCompile(fs, "blit fragment")) {
        // Fragment shader failed — try simple fallback
        std::cerr << "[OutputManager] Uber-shader failed, trying passthrough fallback..." << std::endl;
        static const char* kFallbackFrag = R"GLSL(
#version 330 core
in vec2 vUV;
out vec4 fragColor;
uniform sampler2D uTex;
uniform vec4 uZoneCrop;
void main() {
    vec2 uv = uZoneCrop.xy + vUV * uZoneCrop.zw;
    fragColor = texture(uTex, uv);
}
)GLSL";
        sShaderSource(fs, 1, &kFallbackFrag, nullptr);
        sCompileShader(fs);
        if (!checkShaderCompile(fs, "blit fragment fallback")) {
            if (sDeleteShader) { sDeleteShader(vs); sDeleteShader(fs); }
            return;
        }
    }

    mBlitProg = sCreateProgram();
    sAttachShader(mBlitProg, vs);
    sAttachShader(mBlitProg, fs);
    sLinkProgram(mBlitProg);
    if (!checkProgramLink(mBlitProg)) {
        if (sDeleteProgram) sDeleteProgram(mBlitProg);
        mBlitProg = 0;
        if (sDeleteShader) { sDeleteShader(vs); sDeleteShader(fs); }
        return;
    }
    if (sDeleteShader) {
        sDeleteShader(vs);
        sDeleteShader(fs);
    }
    std::cout << "[OutputManager] Blit shader compiled (prog=" << mBlitProg << ")" << std::endl;

    // Empty VAO — required by GL 3.3 core profile.
    // Vertex data is generated procedurally via gl_VertexID in the vertex shader,
    // avoiding AMD driver issues with VBO data across wglMakeCurrent DC switches.
    sGenVertexArrays(1, &mBlitVAO);
    sBindVertexArray(mBlitVAO);
    sBindVertexArray(0);
}

void OutputManager::blitToWindow(const OutputSlot& slot, unsigned int glTexId) {
    if (mBlitProg == 0) {
        initBlitGL();
        if (mBlitProg == 0) return;
    }
    if (!glTexId) return;

    // Draw to the output window's default framebuffer.
    // Reset GL state that OGRE may have changed — any of these can cause invisible rendering.
    if (sBindFBO) sBindFBO(0x8D40 /*GL_FRAMEBUFFER*/, 0);
    glViewport(0, 0, static_cast<int>(slot.width), static_cast<int>(slot.height));
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_STENCIL_TEST);
    glDisable(GL_BLEND);
    glDisable(GL_CULL_FACE);
    glDisable(GL_SCISSOR_TEST);
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    glDepthMask(GL_FALSE);

    sUseProgram(mBlitProg);
    glBindTexture(GL_TEXTURE_2D, glTexId);

    int locTex = sGetUniformLoc(mBlitProg, "uTex");
    if (locTex >= 0 && sUniform1i)
        sUniform1i(locTex, 0);

    // ── Zone crop: map output to its assigned zone sub-region ────────────────
    float zx = 0.f, zy = 0.f, zw = 1.f, zh = 1.f;
    if (mSurfaceMap && slot.zoneId >= 0) {
        const auto* zone = mSurfaceMap->getZone(slot.zoneId);
        if (zone) {
            zx = zone->x;
            zy = zone->y;
            zw = zone->width;
            zh = zone->height;
        }
    }
    int locCrop = sGetUniformLoc(mBlitProg, "uZoneCrop");
    if (locCrop >= 0 && sUniform4f)
        sUniform4f(locCrop, zx, zy, zw, zh);

    // ── Warp / Blend / Grid uniforms ─────────────────────────────────────────
    // Quad warp
    int locWarpEn = sGetUniformLoc(mBlitProg, "uWarpEnabled");
    if (locWarpEn >= 0 && sUniform1i)
        sUniform1i(locWarpEn, slot.warpEnabled ? 1 : 0);
    if (slot.warpEnabled) {
        const float* c = slot.warpProfile.corners;
        int loc;
        loc = sGetUniformLoc(mBlitProg, "uWarpTL"); if (loc >= 0 && sUniform2f) sUniform2f(loc, c[0], c[1]);
        loc = sGetUniformLoc(mBlitProg, "uWarpTR"); if (loc >= 0 && sUniform2f) sUniform2f(loc, c[2], c[3]);
        loc = sGetUniformLoc(mBlitProg, "uWarpBL"); if (loc >= 0 && sUniform2f) sUniform2f(loc, c[4], c[5]);
        loc = sGetUniformLoc(mBlitProg, "uWarpBR"); if (loc >= 0 && sUniform2f) sUniform2f(loc, c[6], c[7]);
    }

    // Grid warp
    int locGridEn = sGetUniformLoc(mBlitProg, "uGridWarpEnabled");
    if (locGridEn >= 0 && sUniform1i)
        sUniform1i(locGridEn, slot.gridWarpEnabled ? 1 : 0);
    if (slot.gridWarpEnabled) {
        const auto& gw = slot.gridWarpProfile;
        float flatGrid[32];
        for (int i = 0; i < 8; ++i) {
            int a = i * 2, b = i * 2 + 1;
            flatGrid[i * 4 + 0] = gw.pts[a * 2 + 0];
            flatGrid[i * 4 + 1] = gw.pts[a * 2 + 1];
            flatGrid[i * 4 + 2] = gw.pts[b * 2 + 0];
            flatGrid[i * 4 + 3] = gw.pts[b * 2 + 1];
        }
        int locGrid = sGetUniformLoc(mBlitProg, "uGrid");
        if (locGrid >= 0 && sUniform4fv)
            sUniform4fv(locGrid, 8, flatGrid);
    }

    // Edge blend
    int locBlendEn = sGetUniformLoc(mBlitProg, "uBlendEnabled");
    if (locBlendEn >= 0 && sUniform1i)
        sUniform1i(locBlendEn, slot.blendEnabled ? 1 : 0);
    if (slot.blendEnabled) {
        int loc;
        loc = sGetUniformLoc(mBlitProg, "uBlendLeft");   if (loc >= 0 && sUniform1f) sUniform1f(loc, slot.blendProfile.left);
        loc = sGetUniformLoc(mBlitProg, "uBlendRight");  if (loc >= 0 && sUniform1f) sUniform1f(loc, slot.blendProfile.right);
        loc = sGetUniformLoc(mBlitProg, "uBlendTop");    if (loc >= 0 && sUniform1f) sUniform1f(loc, slot.blendProfile.top);
        loc = sGetUniformLoc(mBlitProg, "uBlendBottom"); if (loc >= 0 && sUniform1f) sUniform1f(loc, slot.blendProfile.bottom);
        loc = sGetUniformLoc(mBlitProg, "uBlendGamma");  if (loc >= 0 && sUniform1f) sUniform1f(loc, slot.blendProfile.gamma);
    }

    // ── Draw ─────────────────────────────────────────────────────────────────
    sBindVertexArray(mBlitVAO);
    glDrawArrays(GL_TRIANGLES, 0, 3);  // gl_VertexID fullscreen triangle
    sBindVertexArray(0);
    sUseProgram(0);
    glBindTexture(GL_TEXTURE_2D, 0);

    // ── Restore GL state ─────────────────────────────────────────────────────
    // OGRE's GL3Plus renderer uses an internal state cache.  If we change GL state
    // behind its back (via direct gl* calls), the cache becomes stale and OGRE will
    // not re-issue the correct state on the next frame — causing invisible geometry,
    // broken depth, missing particles, etc.  Restore to OGRE's expected defaults.
    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_TRUE);
    glEnable(GL_CULL_FACE);
    glEnable(GL_BLEND);
}

// ── WarpWizard test pattern GL rendering (v3.4 Lot D) ────────────────────────

/// Vertex shader: clip-space fullscreen triangle.
static const char* kTestPatVert = R"GLSL(
#version 330 core
layout(location = 0) in vec2 aPos;
void main() { gl_Position = vec4(aPos, 0.0, 1.0); }
)GLSL";

/// Fragment shader: procedural grid + red corner circles + green click dots.
static const char* kTestPatFrag = R"GLSL(
#version 330 core
out vec4 fragColor;
uniform vec2  u_resolution;
uniform vec2  u_clicks[4];
uniform int   u_numClicked;

void main() {
    vec2 uv = gl_FragCoord.xy / u_resolution;
    // Flip Y so (0,0)=top-left matches normalised screen coords
    uv.y = 1.0 - uv.y;

    vec3 col = vec3(0.0); // black background

    // White grid (8x8 cells, 2px lines)
    float lineW = 2.0 / min(u_resolution.x, u_resolution.y);
    vec2 grid = fract(uv * 8.0);
    if (grid.x < lineW * 8.0 || grid.y < lineW * 8.0) col = vec3(1.0);

    // Corner arrows / markers: TL, TR, BL, BR in red (20px radius in UV)
    float rC = 20.0 / min(u_resolution.x, u_resolution.y);
    const vec2 corners[4] = vec2[4](
        vec2(0.05, 0.05), vec2(0.95, 0.05),
        vec2(0.05, 0.95), vec2(0.95, 0.95)
    );
    for (int i = 0; i < 4; i++) {
        if (distance(uv, corners[i]) < rC) col = vec3(1.0, 0.0, 0.0);
    }

    // Green dots at clicked positions
    float rG = 12.0 / min(u_resolution.x, u_resolution.y);
    for (int i = 0; i < u_numClicked; i++) {
        if (distance(uv, u_clicks[i]) < rG) col = vec3(0.0, 1.0, 0.0);
    }

    fragColor = vec4(col, 1.0);
}
)GLSL";

void OutputManager::initTestPatternGL() {
    ensureGL3Funcs();
    if (!sCreateShader || !sCreateProgram) return;

    // Compile vertex shader.
    unsigned int vs = sCreateShader(0x8B31 /*GL_VERTEX_SHADER*/);
    sShaderSource(vs, 1, &kTestPatVert, nullptr);
    sCompileShader(vs);

    // Compile fragment shader.
    unsigned int fs = sCreateShader(0x8B30 /*GL_FRAGMENT_SHADER*/);
    sShaderSource(fs, 1, &kTestPatFrag, nullptr);
    sCompileShader(fs);

    // Link program.
    mTestPatternProg = sCreateProgram();
    sAttachShader(mTestPatternProg, vs);
    sAttachShader(mTestPatternProg, fs);
    sLinkProgram(mTestPatternProg);
    if (sDeleteShader) {
        sDeleteShader(vs);
        sDeleteShader(fs);
    }

    // Fullscreen quad (two triangles covering clip space).
    static const float kQuad[] = {
        -1.f, -1.f,   1.f, -1.f,   1.f,  1.f,
        -1.f, -1.f,   1.f,  1.f,  -1.f,  1.f
    };

    sGenVertexArrays(1, &mTestPatternVAO);
    sBindVertexArray(mTestPatternVAO);
    sGenBuffers(1, &mTestPatternVBO);
    sBindBuffer(0x8892 /*GL_ARRAY_BUFFER*/, mTestPatternVBO);
    sBufferData(0x8892, static_cast<ptrdiff_t>(sizeof(kQuad)), kQuad, 0x88B8 /*GL_STATIC_DRAW*/);
    sEnableVAA(0);
    sVertexAttribPtr(0, 2, 0x1406 /*GL_FLOAT*/, GL_FALSE, 2 * sizeof(float), nullptr);
    sBindVertexArray(0);
}

void OutputManager::renderTestPattern(const OutputSlot& slot) {
    if (mTestPatternProg == 0) {
        initTestPatternGL();
        if (mTestPatternProg == 0) return;
    }
    ensureGL3Funcs();

    // Bind default framebuffer of the current output window.
    if (sBindFBO) sBindFBO(0x8D40 /*GL_FRAMEBUFFER*/, 0);
    glViewport(0, 0, static_cast<int>(slot.width), static_cast<int>(slot.height));
    glClearColor(0.f, 0.f, 0.f, 1.f);
    glClear(GL_COLOR_BUFFER_BIT);

    sUseProgram(mTestPatternProg);

    // Uniforms.
    int locRes = sGetUniformLoc(mTestPatternProg, "u_resolution");
    if (locRes >= 0 && sUniform2f)
        sUniform2f(locRes, static_cast<float>(slot.width), static_cast<float>(slot.height));

    int locClicks = sGetUniformLoc(mTestPatternProg, "u_clicks");
    if (locClicks >= 0 && sUniform2fv) {
        // Pass all 4 pairs (unused ones are zero, ignored by shader).
        sUniform2fv(locClicks, 4, slot.testPattern.clickedPoints);
    }

    int locNum = sGetUniformLoc(mTestPatternProg, "u_numClicked");
    if (locNum >= 0 && sUniform1i)
        sUniform1i(locNum, slot.testPattern.numClicked);

    sBindVertexArray(mTestPatternVAO);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    sBindVertexArray(0);

    sUseProgram(0);
}

} // namespace bbfx
