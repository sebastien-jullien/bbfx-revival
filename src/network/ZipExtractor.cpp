#include "network/ZipExtractor.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <iostream>
#include <sstream>

// minizip-ng
#include <mz.h>
#include <mz_strm.h>
#include <mz_zip.h>
#include <mz_zip_rw.h>

namespace fs = std::filesystem;

namespace bbfx {

namespace {

// Normalise a zip entry name to forward-slash separators and reject
// dangerous components. Returns empty string if the name is unsafe.
std::string sanitizeEntryName(const std::string& raw,
                              const ZipExtractor::Options& opts) {
    std::string name = raw;
    // Use forward slashes uniformly (some zippers write backslashes).
    std::replace(name.begin(), name.end(), '\\', '/');

    // Absolute path check.
    if (opts.rejectAbsolutePaths) {
        if (!name.empty() && name.front() == '/') return {};
        // Windows drive letter (e.g. "C:/..." or "C:").
        if (name.size() >= 2 && std::isalpha(static_cast<unsigned char>(name[0]))
            && name[1] == ':') {
            return {};
        }
    }

    // Path traversal check: any "../" component anywhere in the path.
    if (opts.rejectPathTraversal) {
        std::string seg;
        std::istringstream iss(name);
        while (std::getline(iss, seg, '/')) {
            if (seg == "..") return {};
        }
    }

    return name;
}

bool writeEntryContents(void* reader, const fs::path& outPath,
                        int64_t entrySize, std::string& errOut) {
    std::error_code ec;
    fs::create_directories(outPath.parent_path(), ec);
    if (ec) {
        errOut = "cannot create " + outPath.parent_path().string() + ": " + ec.message();
        return false;
    }

    std::ofstream file(outPath, std::ios::binary | std::ios::trunc);
    if (!file.is_open()) {
        errOut = "cannot open for write: " + outPath.string();
        return false;
    }

    if (mz_zip_reader_entry_open(reader) != MZ_OK) {
        errOut = "zip entry_open failed: " + outPath.filename().string();
        return false;
    }

    std::vector<char> buf(64 * 1024);
    int64_t remaining = entrySize > 0 ? entrySize : -1;  // -1 = stream to EOF
    while (true) {
        int32_t want = static_cast<int32_t>(buf.size());
        if (remaining > 0 && remaining < want) want = static_cast<int32_t>(remaining);
        int32_t got = mz_zip_reader_entry_read(reader, buf.data(), want);
        if (got < 0) {
            mz_zip_reader_entry_close(reader);
            errOut = "zip entry_read failed for " + outPath.filename().string();
            return false;
        }
        if (got == 0) break;
        file.write(buf.data(), got);
        if (remaining > 0) remaining -= got;
        if (remaining == 0) break;
    }

    mz_zip_reader_entry_close(reader);
    return true;
}

} // anonymous

bool ZipExtractor::isValidZip(const fs::path& zipPath) {
    void* reader = mz_zip_reader_create();
    if (!reader) return false;
    bool ok = mz_zip_reader_open_file(reader, zipPath.string().c_str()) == MZ_OK;
    if (ok) mz_zip_reader_close(reader);
    mz_zip_reader_delete(&reader);
    return ok;
}

bool ZipExtractor::extract(const fs::path& zipPath,
                           const fs::path& destDir,
                           const Options& opts,
                           std::string& errorMessage) {
    errorMessage.clear();

    std::error_code ec;
    if (!fs::exists(zipPath, ec)) {
        errorMessage = "zip not found: " + zipPath.string();
        return false;
    }

    if (fs::exists(destDir, ec)) {
        if (!fs::is_directory(destDir, ec)) {
            errorMessage = "destination exists and is not a directory: " + destDir.string();
            return false;
        }
        if (!opts.overwriteExisting && !fs::is_empty(destDir, ec)) {
            errorMessage = "destination is not empty: " + destDir.string();
            return false;
        }
    } else {
        fs::create_directories(destDir, ec);
        if (ec) {
            errorMessage = "cannot create destination: " + ec.message();
            return false;
        }
    }

    void* reader = mz_zip_reader_create();
    if (!reader) {
        errorMessage = "mz_zip_reader_create failed";
        return false;
    }

    int32_t err = mz_zip_reader_open_file(reader, zipPath.string().c_str());
    if (err != MZ_OK) {
        mz_zip_reader_delete(&reader);
        errorMessage = "not a valid zip: " + zipPath.string();
        return false;
    }

    int64_t totalUncompressed = 0;

    // First pass: walk every entry and validate names + sizes. We don't
    // write anything yet — if any entry is unsafe we refuse up front.
    err = mz_zip_reader_goto_first_entry(reader);
    while (err == MZ_OK) {
        mz_zip_file* info = nullptr;
        if (mz_zip_reader_entry_get_info(reader, &info) != MZ_OK || !info) {
            errorMessage = "cannot read entry info";
            mz_zip_reader_close(reader);
            mz_zip_reader_delete(&reader);
            return false;
        }

        std::string rawName = info->filename ? std::string(info->filename) : std::string();
        std::string safe = sanitizeEntryName(rawName, opts);
        if (safe.empty()) {
            errorMessage = "unsafe entry name: \"" + rawName + "\"";
            mz_zip_reader_close(reader);
            mz_zip_reader_delete(&reader);
            return false;
        }

        if (info->uncompressed_size > opts.maxEntryUncompressedSize) {
            std::ostringstream oss;
            oss << "entry too large (" << info->uncompressed_size << " bytes > "
                << opts.maxEntryUncompressedSize << "): " << rawName;
            errorMessage = oss.str();
            mz_zip_reader_close(reader);
            mz_zip_reader_delete(&reader);
            return false;
        }

        totalUncompressed += info->uncompressed_size;
        if (totalUncompressed > opts.maxUncompressedSize) {
            std::ostringstream oss;
            oss << "total uncompressed size exceeds " << opts.maxUncompressedSize
                << " bytes (zip bomb guard)";
            errorMessage = oss.str();
            mz_zip_reader_close(reader);
            mz_zip_reader_delete(&reader);
            return false;
        }

        err = mz_zip_reader_goto_next_entry(reader);
    }
    if (err != MZ_OK && err != MZ_END_OF_LIST) {
        errorMessage = "zip walk failed";
        mz_zip_reader_close(reader);
        mz_zip_reader_delete(&reader);
        return false;
    }

    // Second pass: actually extract. Names are known safe.
    err = mz_zip_reader_goto_first_entry(reader);
    while (err == MZ_OK) {
        mz_zip_file* info = nullptr;
        if (mz_zip_reader_entry_get_info(reader, &info) != MZ_OK || !info) break;

        std::string rawName = info->filename ? std::string(info->filename) : std::string();
        std::string safe = sanitizeEntryName(rawName, opts);
        // sanitize already passed in the validation pass — sanity.
        if (safe.empty()) { err = MZ_END_OF_LIST; break; }

        fs::path out = destDir / safe;

        if (mz_zip_reader_entry_is_dir(reader) == MZ_OK) {
            fs::create_directories(out, ec);
            err = mz_zip_reader_goto_next_entry(reader);
            continue;
        }

        std::string w_err;
        if (!writeEntryContents(reader, out, info->uncompressed_size, w_err)) {
            errorMessage = w_err.empty() ? ("write failed: " + safe) : w_err;
            mz_zip_reader_close(reader);
            mz_zip_reader_delete(&reader);
            return false;
        }

        err = mz_zip_reader_goto_next_entry(reader);
    }

    mz_zip_reader_close(reader);
    mz_zip_reader_delete(&reader);
    return true;
}

} // namespace bbfx
