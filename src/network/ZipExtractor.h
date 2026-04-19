#pragma once

#include <filesystem>
#include <string>

namespace bbfx {

/// v3.5 Lot F — ZIP extraction backed by minizip-ng.
///
/// Safety guarantees (every one enforced per-entry before any bytes are
/// written to disk):
///   - reject entries whose name contains '..' (path traversal)
///   - reject entries whose name is an absolute path (leading '/', '\\',
///     or a Windows drive letter such as "C:")
///   - refuse to extract once the total uncompressed size would exceed
///     `maxUncompressedSize` (zip bomb protection)
///   - refuse to extract once per-entry uncompressed size exceeds
///     `maxEntryUncompressedSize`
///
/// A single `extract` call is atomic from the caller's perspective:
///   - if ANY entry is rejected, the destination directory is left as-is.
///     Caller is expected to clean temp dirs when error_out is non-empty.
///
/// Symlinks and file permissions are not preserved — we always create
/// regular files. Plugins are Lua + GLSL so symlinks are unnecessary.
class ZipExtractor {
public:
    struct Options {
        int64_t maxUncompressedSize      = 100LL * 1024 * 1024; // 100 MB total
        int64_t maxEntryUncompressedSize = 50LL  * 1024 * 1024; // 50 MB per file
        bool    rejectAbsolutePaths      = true;
        bool    rejectPathTraversal      = true;
        // If true and the destination directory already exists, extract
        // into it anyway (files are overwritten). If false and the dir
        // already has files, `extract` fails up-front.
        bool    overwriteExisting        = false;
    };

    /// Extracts `zipPath` into `destDir`. Returns true on full success.
    /// On failure, `errorMessage` is populated and partial writes may be
    /// left behind — the caller is expected to `std::filesystem::remove_all`
    /// `destDir` on a fresh temp directory.
    static bool extract(const std::filesystem::path& zipPath,
                        const std::filesystem::path& destDir,
                        const Options& opts,
                        std::string& errorMessage);

    /// Lightweight sanity check: does the file open as a ZIP archive?
    static bool isValidZip(const std::filesystem::path& zipPath);
};

} // namespace bbfx
