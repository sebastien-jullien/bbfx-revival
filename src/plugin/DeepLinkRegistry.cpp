#include "plugin/DeepLinkRegistry.h"

#include <iostream>

#ifdef _WIN32
#include <windows.h>
#endif

namespace bbfx {

#ifdef _WIN32
namespace {

// Write a (REG_SZ) string value. Creates the key if needed.
bool writeReg(HKEY root, const std::wstring& subkey,
              const std::wstring& valueName, const std::wstring& data) {
    HKEY h = nullptr;
    LONG r = RegCreateKeyExW(root, subkey.c_str(), 0, nullptr,
                              REG_OPTION_NON_VOLATILE, KEY_WRITE,
                              nullptr, &h, nullptr);
    if (r != ERROR_SUCCESS) return false;
    r = RegSetValueExW(h, valueName.empty() ? nullptr : valueName.c_str(),
                       0, REG_SZ,
                       reinterpret_cast<const BYTE*>(data.c_str()),
                       static_cast<DWORD>((data.size() + 1) * sizeof(wchar_t)));
    RegCloseKey(h);
    return r == ERROR_SUCCESS;
}

std::wstring toWide(const std::string& s) {
    if (s.empty()) return {};
    int n = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, nullptr, 0);
    std::wstring w(n ? n - 1 : 0, L'\0');
    if (n > 1) MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, w.data(), n);
    return w;
}

} // anonymous
#endif

bool DeepLinkRegistry::registerScheme(const std::string& exePath) {
#ifdef _WIN32
    std::wstring we = toWide(exePath);
    if (we.empty()) return false;

    const std::wstring base = L"Software\\Classes\\bbfx";
    bool ok = true;
    ok &= writeReg(HKEY_CURRENT_USER, base, L"",
                   L"URL:BBFx Protocol");
    ok &= writeReg(HKEY_CURRENT_USER, base, L"URL Protocol", L"");
    ok &= writeReg(HKEY_CURRENT_USER, base + L"\\DefaultIcon", L"",
                   L"\"" + we + L"\",1");
    // %1 is substituted with the URL by the OS.
    ok &= writeReg(HKEY_CURRENT_USER, base + L"\\shell\\open\\command", L"",
                   L"\"" + we + L"\" --deep-link \"%1\"");
    if (!ok) {
        std::cerr << "[DeepLinkRegistry] failed to write HKCU\\Software\\Classes\\bbfx"
                  << std::endl;
    }
    return ok;
#else
    (void)exePath;
    return false;
#endif
}

bool DeepLinkRegistry::isSchemeRegistered() {
#ifdef _WIN32
    HKEY h = nullptr;
    LONG r = RegOpenKeyExW(HKEY_CURRENT_USER,
                            L"Software\\Classes\\bbfx\\shell\\open\\command",
                            0, KEY_READ, &h);
    if (r != ERROR_SUCCESS) return false;
    RegCloseKey(h);
    return true;
#else
    return false;
#endif
}

} // namespace bbfx
