#include "ScriptPanelRegistry.h"

#include <algorithm>
#include <iostream>

#include <imgui.h>

namespace bbfx {

ScriptPanelRegistry& ScriptPanelRegistry::instance() {
    static ScriptPanelRegistry inst;
    return inst;
}

void ScriptPanelRegistry::registerPanel(const std::string& pluginId,
                                              const std::string& title,
                                              sol::function drawFn) {
    if (title.empty() || !drawFn.valid()) return;
    std::lock_guard<std::mutex> lock(mMutex);
    // Upsert: if a panel with that title already exists, replace the
    // callback but keep the visible flag (so hot-reload doesn't flicker).
    for (auto& e : mPanels) {
        if (e.title == title) {
            e.drawFn = std::move(drawFn);
            e.pluginId = pluginId;
            return;
        }
    }
    mPanels.push_back(Entry{ pluginId, title, std::move(drawFn), true });
}

void ScriptPanelRegistry::unregisterPanel(const std::string& title) {
    std::lock_guard<std::mutex> lock(mMutex);
    mPanels.erase(std::remove_if(mPanels.begin(), mPanels.end(),
        [&title](const Entry& e) { return e.title == title; }),
        mPanels.end());
}

void ScriptPanelRegistry::unregisterByPlugin(const std::string& pluginId) {
    std::lock_guard<std::mutex> lock(mMutex);
    mPanels.erase(std::remove_if(mPanels.begin(), mPanels.end(),
        [&pluginId](const Entry& e) { return e.pluginId == pluginId; }),
        mPanels.end());
}

void ScriptPanelRegistry::drawAll() {
    std::vector<Entry> snapshot;
    {
        std::lock_guard<std::mutex> lock(mMutex);
        snapshot = mPanels;
    }
    for (auto& e : snapshot) {
        if (!e.visible || !e.drawFn.valid()) continue;
        bool open = true;
        if (ImGui::Begin(e.title.c_str(), &open)) {
            try {
                e.drawFn();
            } catch (const std::exception& ex) {
                ImGui::TextColored({1.0f, 0.3f, 0.3f, 1.0f}, "Panel callback error:");
                ImGui::TextWrapped("%s", ex.what());
                std::cerr << "[ScriptPanel] " << e.title << ": " << ex.what() << std::endl;
            }
        }
        ImGui::End();
        if (!open) {
            // User clicked X on the window — propagate into the entry.
            std::lock_guard<std::mutex> lock(mMutex);
            for (auto& live : mPanels) {
                if (live.title == e.title) { live.visible = false; break; }
            }
        }
    }
}

std::vector<std::string> ScriptPanelRegistry::titles() const {
    std::lock_guard<std::mutex> lock(mMutex);
    std::vector<std::string> out;
    out.reserve(mPanels.size());
    for (auto& e : mPanels) out.push_back(e.title);
    return out;
}

bool ScriptPanelRegistry::toggleVisible(const std::string& title) {
    std::lock_guard<std::mutex> lock(mMutex);
    for (auto& e : mPanels) {
        if (e.title == title) {
            e.visible = !e.visible;
            return e.visible;
        }
    }
    return false;
}

} // namespace bbfx
