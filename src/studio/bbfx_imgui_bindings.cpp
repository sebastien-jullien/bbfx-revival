#include "bbfx_imgui_bindings.h"

#include <cstdint>
#include <string>
#include <vector>

#include <imgui.h>

#include <OgreTexture.h>
#include <OgreTextureManager.h>

#include "ScriptPanelRegistry.h"
#include "../plugin/InspectorWidgetRegistry.h"
#include "../plugin/PluginManager.h"
#include "../core/ParamSpec.h"

namespace bbfx {

namespace {

ImTextureID resolveOgreTextureID(const std::string& name) {
    auto& tm = Ogre::TextureManager::getSingleton();
    auto tex = tm.getByName(name);
    if (!tex) {
        // Try loading on-demand from the general group. Silently fall
        // through to 0 if nothing matches.
        try { tex = tm.load(name, Ogre::ResourceGroupManager::DEFAULT_RESOURCE_GROUP_NAME); }
        catch (...) { return static_cast<ImTextureID>(0); }
    }
    if (!tex) return static_cast<ImTextureID>(0);
    unsigned int glId = 0;
    tex->getCustomAttribute("GLID", &glId);
    return static_cast<ImTextureID>(static_cast<uintptr_t>(glId));
}

std::vector<const char*> toCStrings(const sol::table& items,
                                       std::vector<std::string>& storage) {
    storage.clear();
    for (auto& kv : items) {
        if (kv.second.is<std::string>()) storage.push_back(kv.second.as<std::string>());
    }
    std::vector<const char*> ptrs;
    ptrs.reserve(storage.size());
    for (auto& s : storage) ptrs.push_back(s.c_str());
    return ptrs;
}

// Current plugin id for the sandbox-wrap: set by the sandbox before the
// plugin's init() runs, cleared after. Used by registerInspectorWidget
// so wildcard registrations are attributed to the right plugin.
thread_local std::string tCurrentPluginId;

} // anonymous

void registerImguiBindings(sol::state& lua) {
    sol::table bbfx = lua["bbfx"];
    sol::table ui = bbfx.get_or("ui", sol::table(lua, sol::create));
    bbfx["ui"] = ui;

    // ── Text ────────────────────────────────────────────────────────────
    ui["text"]         = [](const std::string& s) { ImGui::TextUnformatted(s.c_str()); };
    ui["textColored"]  = [](float r, float g, float b, float a, const std::string& s) {
        ImGui::TextColored(ImVec4(r, g, b, a), "%s", s.c_str());
    };
    ui["textWrapped"]  = [](const std::string& s) { ImGui::TextWrapped("%s", s.c_str()); };
    ui["textDisabled"] = [](const std::string& s) { ImGui::TextDisabled("%s", s.c_str()); };
    ui["bulletText"]   = [](const std::string& s) { ImGui::BulletText("%s", s.c_str()); };

    // ── Buttons ─────────────────────────────────────────────────────────
    ui["button"] = [](const std::string& label, sol::optional<float> w,
                       sol::optional<float> h) -> bool {
        return ImGui::Button(label.c_str(),
                              ImVec2(w.value_or(0.0f), h.value_or(0.0f)));
    };
    ui["smallButton"] = [](const std::string& label) -> bool {
        return ImGui::SmallButton(label.c_str());
    };
    ui["checkbox"] = [](const std::string& label, bool v) -> std::tuple<bool, bool> {
        bool copy = v;
        bool changed = ImGui::Checkbox(label.c_str(), &copy);
        return { changed, copy };
    };
    ui["radioButton"] = [](const std::string& label, bool active) -> bool {
        return ImGui::RadioButton(label.c_str(), active);
    };

    // ── Sliders / Inputs ────────────────────────────────────────────────
    ui["sliderFloat"] = [](const std::string& label, float v, float vmin, float vmax)
                            -> std::tuple<bool, float> {
        float copy = v;
        bool changed = ImGui::SliderFloat(label.c_str(), &copy, vmin, vmax);
        return { changed, copy };
    };
    ui["sliderInt"] = [](const std::string& label, int v, int vmin, int vmax)
                            -> std::tuple<bool, int> {
        int copy = v;
        bool changed = ImGui::SliderInt(label.c_str(), &copy, vmin, vmax);
        return { changed, copy };
    };
    ui["sliderFloat2"] = [](const std::string& label, float x, float y,
                              float vmin, float vmax)
                            -> std::tuple<bool, float, float> {
        float a[2] = { x, y };
        bool changed = ImGui::SliderFloat2(label.c_str(), a, vmin, vmax);
        return { changed, a[0], a[1] };
    };
    ui["sliderFloat3"] = [](const std::string& label, float x, float y, float z,
                              float vmin, float vmax)
                            -> std::tuple<bool, float, float, float> {
        float a[3] = { x, y, z };
        bool changed = ImGui::SliderFloat3(label.c_str(), a, vmin, vmax);
        return { changed, a[0], a[1], a[2] };
    };
    ui["sliderFloat4"] = [](const std::string& label, float x, float y, float z, float w,
                              float vmin, float vmax)
                            -> std::tuple<bool, float, float, float, float> {
        float a[4] = { x, y, z, w };
        bool changed = ImGui::SliderFloat4(label.c_str(), a, vmin, vmax);
        return { changed, a[0], a[1], a[2], a[3] };
    };
    ui["inputFloat"] = [](const std::string& label, float v, sol::optional<float> step)
                            -> std::tuple<bool, float> {
        float copy = v;
        bool changed = ImGui::InputFloat(label.c_str(), &copy, step.value_or(0.0f));
        return { changed, copy };
    };
    ui["inputInt"] = [](const std::string& label, int v, sol::optional<int> step)
                            -> std::tuple<bool, int> {
        int copy = v;
        bool changed = ImGui::InputInt(label.c_str(), &copy, step.value_or(1));
        return { changed, copy };
    };
    ui["inputText"] = [](const std::string& label, const std::string& v,
                          sol::optional<int> maxLen)
                            -> std::tuple<bool, std::string> {
        int cap = maxLen.value_or(256);
        std::vector<char> buf(cap + 1, 0);
        std::size_t n = std::min<std::size_t>(v.size(), static_cast<std::size_t>(cap));
        std::memcpy(buf.data(), v.data(), n);
        bool changed = ImGui::InputText(label.c_str(), buf.data(), buf.size());
        return { changed, std::string(buf.data()) };
    };

    // ── Color ───────────────────────────────────────────────────────────
    ui["colorEdit3"] = [](const std::string& label, float r, float g, float b)
                            -> std::tuple<bool, float, float, float> {
        float c[3] = { r, g, b };
        bool changed = ImGui::ColorEdit3(label.c_str(), c);
        return { changed, c[0], c[1], c[2] };
    };
    ui["colorEdit4"] = [](const std::string& label, float r, float g, float b, float a)
                            -> std::tuple<bool, float, float, float, float> {
        float c[4] = { r, g, b, a };
        bool changed = ImGui::ColorEdit4(label.c_str(), c);
        return { changed, c[0], c[1], c[2], c[3] };
    };
    ui["colorPicker3"] = [](const std::string& label, float r, float g, float b)
                            -> std::tuple<bool, float, float, float> {
        float c[3] = { r, g, b };
        bool changed = ImGui::ColorPicker3(label.c_str(), c);
        return { changed, c[0], c[1], c[2] };
    };

    // ── Combo / ListBox ─────────────────────────────────────────────────
    ui["combo"] = [](const std::string& label, int currentItem, sol::table items)
                      -> std::tuple<bool, int> {
        std::vector<std::string> storage;
        auto ptrs = toCStrings(items, storage);
        int copy = currentItem;
        bool changed = ImGui::Combo(label.c_str(), &copy,
                                      ptrs.empty() ? nullptr : ptrs.data(),
                                      static_cast<int>(ptrs.size()));
        return { changed, copy };
    };
    ui["listBox"] = [](const std::string& label, int currentItem, sol::table items,
                        sol::optional<int> heightInItems)
                      -> std::tuple<bool, int> {
        std::vector<std::string> storage;
        auto ptrs = toCStrings(items, storage);
        int copy = currentItem;
        bool changed = ImGui::ListBox(label.c_str(), &copy,
                                        ptrs.empty() ? nullptr : ptrs.data(),
                                        static_cast<int>(ptrs.size()),
                                        heightInItems.value_or(4));
        return { changed, copy };
    };

    // ── Layout ──────────────────────────────────────────────────────────
    ui["separator"] = []() { ImGui::Separator(); };
    ui["spacing"]   = []() { ImGui::Spacing();   };
    ui["sameLine"]  = [](sol::optional<float> offset, sol::optional<float> spacing) {
        ImGui::SameLine(offset.value_or(0.0f), spacing.value_or(-1.0f));
    };
    ui["newLine"]   = []() { ImGui::NewLine(); };
    ui["indent"]    = [](sol::optional<float> w) { ImGui::Indent(w.value_or(0.0f)); };
    ui["unindent"]  = [](sol::optional<float> w) { ImGui::Unindent(w.value_or(0.0f)); };
    ui["columns"]   = [](int n) { ImGui::Columns(n); };
    ui["nextColumn"]= []() { ImGui::NextColumn(); };
    ui["treeNode"]  = [](const std::string& label) -> bool {
        return ImGui::TreeNode(label.c_str());
    };
    ui["treePop"]   = []() { ImGui::TreePop(); };
    ui["collapsingHeader"] = [](const std::string& label) -> bool {
        return ImGui::CollapsingHeader(label.c_str());
    };

    // ── Image + plots ───────────────────────────────────────────────────
    ui["image"] = [](const std::string& textureName, float w, float h) {
        ImTextureID id = resolveOgreTextureID(textureName);
        if (id == static_cast<ImTextureID>(0)) {
            ImGui::TextDisabled("<texture '%s' not found>", textureName.c_str());
            return;
        }
        ImGui::Image(id, ImVec2(w, h));
    };
    ui["plotLines"] = [](const std::string& label, sol::table values,
                          sol::optional<std::string> overlay,
                          sol::optional<float> scaleMin,
                          sol::optional<float> scaleMax,
                          sol::optional<float> sizeX,
                          sol::optional<float> sizeY) {
        std::vector<float> buf;
        for (auto& kv : values) {
            if (kv.second.is<double>()) buf.push_back(static_cast<float>(kv.second.as<double>()));
            else if (kv.second.is<int>()) buf.push_back(static_cast<float>(kv.second.as<int>()));
        }
        ImGui::PlotLines(label.c_str(), buf.data(), static_cast<int>(buf.size()), 0,
                          overlay ? overlay->c_str() : nullptr,
                          scaleMin.value_or(FLT_MAX),
                          scaleMax.value_or(FLT_MAX),
                          ImVec2(sizeX.value_or(0.0f), sizeY.value_or(0.0f)));
    };
    ui["plotHistogram"] = [](const std::string& label, sol::table values,
                                sol::optional<std::string> overlay,
                                sol::optional<float> scaleMin,
                                sol::optional<float> scaleMax,
                                sol::optional<float> sizeX,
                                sol::optional<float> sizeY) {
        std::vector<float> buf;
        for (auto& kv : values) {
            if (kv.second.is<double>()) buf.push_back(static_cast<float>(kv.second.as<double>()));
            else if (kv.second.is<int>()) buf.push_back(static_cast<float>(kv.second.as<int>()));
        }
        ImGui::PlotHistogram(label.c_str(), buf.data(), static_cast<int>(buf.size()), 0,
                              overlay ? overlay->c_str() : nullptr,
                              scaleMin.value_or(FLT_MAX),
                              scaleMax.value_or(FLT_MAX),
                              ImVec2(sizeX.value_or(0.0f), sizeY.value_or(0.0f)));
    };
    ui["progressBar"] = [](float fraction, sol::optional<std::string> overlay,
                              sol::optional<float> sizeX,
                              sol::optional<float> sizeY) {
        ImGui::ProgressBar(fraction,
                            ImVec2(sizeX.value_or(-1.0f), sizeY.value_or(0.0f)),
                            overlay ? overlay->c_str() : nullptr);
    };

    // ── Tabs / popups / tooltips ────────────────────────────────────────
    ui["beginTabBar"]  = [](const std::string& id) -> bool {
        return ImGui::BeginTabBar(id.c_str());
    };
    ui["endTabBar"]    = []() { ImGui::EndTabBar(); };
    ui["beginTabItem"] = [](const std::string& label) -> bool {
        return ImGui::BeginTabItem(label.c_str());
    };
    ui["endTabItem"]   = []() { ImGui::EndTabItem(); };
    ui["beginPopup"]   = [](const std::string& id) -> bool {
        return ImGui::BeginPopup(id.c_str());
    };
    ui["endPopup"]     = []() { ImGui::EndPopup(); };
    ui["openPopup"]    = [](const std::string& id) { ImGui::OpenPopup(id.c_str()); };
    ui["closeCurrentPopup"] = []() { ImGui::CloseCurrentPopup(); };
    ui["tooltip"]      = [](const std::string& s) {
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", s.c_str());
    };

    // ── Registration API — panels & inspector widgets ───────────────────
    ui["registerPanel"] = [](const std::string& title, sol::function drawFn) {
        ScriptPanelRegistry::instance().registerPanel(tCurrentPluginId, title, std::move(drawFn));
    };
    ui["unregisterPanel"] = [](const std::string& title) {
        ScriptPanelRegistry::instance().unregisterPanel(title);
    };
    ui["registerInspectorWidget"] = [](const std::string& portName, sol::function drawFn) {
        // Wildcard registration (matches any node type that has a port
        // with that name). Narrow node-type-scoped registration lands
        // later if needed — the registry already supports it.
        InspectorWidgetRegistry::instance().registerForPortWildcard(
            tCurrentPluginId, portName,
            [drawFn](const std::string& nodeName, const std::string& portName,
                      ParamDef& param) -> bool {
                try {
                    // Pass the current value and expect (changed, newValue).
                    sol::protected_function_result r;
                    if (param.type == ParamType::FLOAT) {
                        r = drawFn(nodeName, portName, param.floatVal);
                        if (r.valid()) {
                            auto ret = r.get<std::tuple<bool, float>>();
                            if (std::get<0>(ret)) param.floatVal = std::get<1>(ret);
                            return true;
                        }
                    } else if (param.type == ParamType::INT) {
                        r = drawFn(nodeName, portName, param.intVal);
                        if (r.valid()) {
                            auto ret = r.get<std::tuple<bool, int>>();
                            if (std::get<0>(ret)) param.intVal = std::get<1>(ret);
                            return true;
                        }
                    } else if (param.type == ParamType::STRING) {
                        r = drawFn(nodeName, portName, param.stringVal);
                        if (r.valid()) {
                            auto ret = r.get<std::tuple<bool, std::string>>();
                            if (std::get<0>(ret)) param.stringVal = std::get<1>(ret);
                            return true;
                        }
                    } else {
                        r = drawFn(nodeName, portName);
                        if (r.valid()) return true;
                    }
                } catch (const std::exception& e) {
                    std::cerr << "[bbfx.ui] inspector widget cb error on "
                                << portName << ": " << e.what() << std::endl;
                }
                return false;
            });
    };

    // Per-plugin teardown helper — called by PluginManager on disable.
    // We expose it as a free function so plugin lifecycle code can strip
    // all UI registrations for a plugin in one call.
    ui["_unregisterAllForPlugin"] = [](const std::string& pluginId) {
        ScriptPanelRegistry::instance().unregisterByPlugin(pluginId);
        InspectorWidgetRegistry::instance().unregisterByPlugin(pluginId);
    };

    // Exposed so the sandbox wrapper can set the current-plugin context
    // around a plugin's init() call. Not called by user Lua.
    ui["_setCurrentPluginId"] = [](const std::string& id) {
        tCurrentPluginId = id;
    };
    ui["_clearCurrentPluginId"] = []() {
        tCurrentPluginId.clear();
    };
}

} // namespace bbfx
