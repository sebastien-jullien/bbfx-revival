#include "plugin/PluginValidator.h"
#include "core/Version.h"   // BBFX_VERSION_STRING — source de vérité unique

#include <fstream>

namespace fs = std::filesystem;

namespace bbfx {

const char* PluginValidator::currentBBFxVersion() {
    // Dérivé de la source de vérité unique (src/core/Version.h) — plus de chaîne
    // hardcodée à bumper manuellement : le gate plugin suit la version de l'app.
    return BBFX_VERSION_STRING;
}

// C12 — empêche le path-traversal : un manifest hostile ne doit pas pouvoir
// référencer un chemin absolu ou remonter hors du dossier du plugin via `..`.
// Retourne true si `rel` reste strictement contenu dans `base`.
static bool isContained(const fs::path& base, const std::string& rel, fs::path& outResolved) {
    fs::path relP(rel);
    if (relP.is_absolute()) return false;                 // chemin absolu interdit
    for (const auto& part : relP)                          // tout segment ".." interdit
        if (part == "..") return false;
    std::error_code ec;
    fs::path canonBase = fs::weakly_canonical(base, ec);
    if (ec) canonBase = base.lexically_normal();
    fs::path candidate = fs::weakly_canonical(base / relP, ec);
    if (ec) candidate = (base / relP).lexically_normal();
    outResolved = candidate;
    // Vérifie que canonBase est un préfixe de candidate.
    auto rel2 = candidate.lexically_relative(canonBase);
    if (rel2.empty()) return false;
    return rel2.native().rfind(L"..", 0) != 0 && rel2 != fs::path("..");
}

static void checkFileExists(const fs::path& base, const std::string& rel,
                            const char* kind, PluginValidator::Result& r) {
    if (rel.empty()) return;
    fs::path p;
    if (!isContained(base, rel, p)) {
        r.ok = false;
        r.errors.push_back(std::string(kind) + " path escapes plugin directory (rejected): " + rel);
        return;
    }
    std::error_code ec;
    if (!fs::exists(p, ec) || !fs::is_regular_file(p, ec)) {
        r.ok = false;
        r.errors.push_back(std::string(kind) + " not found: " + rel);
    }
}

PluginValidator::Result PluginValidator::validate(const fs::path& pluginDir,
                                                  const PluginManifest& m) {
    Result r;

    if (!m.semverSatisfies(currentBBFxVersion())) {
        r.ok = false;
        r.errors.push_back(std::string("bbfx_version constraint '") + m.bbfxVersion +
                           "' not satisfied by current " + currentBBFxVersion());
    }

    checkFileExists(pluginDir, m.entry, "entry", r);

    for (const auto& f : m.nodeFiles)         checkFileExists(pluginDir, f, "node file", r);
    for (const auto& f : m.presetFiles)       checkFileExists(pluginDir, f, "preset file", r);
    for (const auto& f : m.resources.shaders) checkFileExists(pluginDir, f, "shader", r);
    for (const auto& f : m.resources.textures) checkFileExists(pluginDir, f, "texture", r);
    for (const auto& f : m.resources.materials) checkFileExists(pluginDir, f, "material", r);
    for (const auto& f : m.resources.particles) checkFileExists(pluginDir, f, "particle template", r);

    return r;
}

PluginValidator::Result PluginValidator::validatePath(const fs::path& pluginDir) {
    Result r;
    std::error_code ec;
    if (!fs::is_directory(pluginDir, ec)) {
        r.ok = false;
        r.errors.push_back("plugin directory does not exist: " + pluginDir.string());
        return r;
    }

    fs::path manifestPath = pluginDir / "manifest.json";
    if (!fs::exists(manifestPath, ec)) {
        r.ok = false;
        r.errors.push_back("manifest.json missing");
        return r;
    }

    std::ifstream f(manifestPath);
    if (!f.is_open()) {
        r.ok = false;
        r.errors.push_back("cannot open manifest.json");
        return r;
    }

    nlohmann::json j;
    try {
        f >> j;
    } catch (const std::exception& e) {
        r.ok = false;
        r.errors.push_back(std::string("manifest.json parse error: ") + e.what());
        return r;
    }

    std::string parseErr;
    auto manifestOpt = PluginManifest::fromJson(j, parseErr);
    if (!manifestOpt) {
        r.ok = false;
        r.errors.push_back("manifest schema error: " + parseErr);
        return r;
    }

    return validate(pluginDir, *manifestOpt);
}

} // namespace bbfx
