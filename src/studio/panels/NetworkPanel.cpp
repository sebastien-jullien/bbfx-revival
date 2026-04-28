#include "NetworkPanel.h"
#include <imgui.h>
#include <iostream>
#include <numeric>

namespace bbfx {

// ── Public API ────────────────────────────────────────────────────────────────

void NetworkPanel::pushLog(const std::string& msg) {
    LogEntry e;
    e.text = msg;
    e.ts   = std::chrono::steady_clock::now();
    mLog.push_back(std::move(e));
    if (static_cast<int>(mLog.size()) > kMaxLog) mLog.pop_front();
}

void NetworkPanel::recordLatency(const std::string& ip, float latencyMs) {
    auto* h = findOrCreateHistory(ip);
    h->samples.push_back(latencyMs);
    if (static_cast<int>(h->samples.size()) > LatencyHistory::kMaxSamples) h->samples.pop_front();
}

NetworkPanel::LatencyHistory* NetworkPanel::findOrCreateHistory(const std::string& ip) {
    for (auto& h : mLatencyHistories)
        if (h.ip == ip) return &h;
    mLatencyHistories.push_back({ip, {}});
    return &mLatencyHistories.back();
}

// ── Main render ───────────────────────────────────────────────────────────────

void NetworkPanel::render(SyncManager* sync, bool* pOpen) {
    if (!pOpen || !*pOpen) return;

    ImGui::SetNextWindowSize(ImVec2(560, 500), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Network Sync", pOpen)) {
        ImGui::End();
        return;
    }

    if (!sync) {
        ImGui::TextDisabled("SyncManager not available.");
        ImGui::End();
        return;
    }

    renderToolbar(sync);
    renderPeerTable(sync);

    if (mSelectedPeerIdx >= 0) {
        ImGui::Spacing();
        renderPeerDetail(sync);
    }

    ImGui::Spacing();
    renderLog();

    ImGui::End();
}

// ── Toolbar: role, start/stop, discovery, manual connect ─────────────────────

void NetworkPanel::renderToolbar(SyncManager* sync) {
    ImGui::SeparatorText("Role");

    SyncRole currentRole = sync->getRole();
    const char* roles[] = { "Standalone", "Master", "Slave" };
    int roleIdx = static_cast<int>(currentRole);

    ImGui::SetNextItemWidth(170.f);
    if (ImGui::Combo("##role", &roleIdx, roles, 3)) {
        SyncRole newRole = static_cast<SyncRole>(roleIdx);
        if (newRole != currentRole) {
            sync->setRole(newRole);
            pushLog(std::string("Role changed to ") + syncRoleName(newRole));
        }
    }
    ImGui::SameLine();

    if (sync->isRunning()) {
        ImGui::TextColored(ImVec4(0.2f, 1.f, 0.2f, 1.f), "%s", syncRoleName(sync->getRole()));
        ImGui::SameLine();
        if (ImGui::SmallButton("Stop")) {
            sync->stop();
            pushLog("Sync stopped.");
        }
    } else {
        ImGui::TextColored(ImVec4(1.f, 0.5f, 0.f, 1.f), "Stopped");
        ImGui::SameLine();
        if (ImGui::SmallButton("Start")) {
            sync->start();
            pushLog(std::string("Sync started as ") + syncRoleName(sync->getRole()));
        }
    }

    // Clock offset (slave only)
    if (currentRole == SyncRole::SLAVE && sync->isRunning()) {
        ImGui::SameLine();
        ImGui::TextDisabled("  Clock offset: %.1f ms", sync->getClockOffset());
    }

    // Auto-discovery (not yet implemented — SyncManager has no discovery API)
    ImGui::SeparatorText("Discovery");
    ImGui::BeginDisabled(true);
    ImGui::Checkbox("Auto-discovery (UDP broadcast)", &mAutoDiscovery);
    ImGui::EndDisabled();
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
        ImGui::SetTooltip("Not yet implemented — use manual IP connection below");

    // Manual connect
    ImGui::BeginDisabled(false);
    ImGui::SetNextItemWidth(160.f);
    ImGui::InputText("IP##manual", mManualIpBuf, sizeof(mManualIpBuf));
    ImGui::SameLine();
    if (ImGui::Button("Connect") && mManualIpBuf[0]) {
        sync->addManualPeer(mManualIpBuf);
        pushLog(std::string("Manual connect to ") + mManualIpBuf);
    }
    ImGui::EndDisabled();

    // Master controls
    if (currentRole == SyncRole::MASTER && sync->isRunning()) {
        ImGui::SeparatorText("Master Controls");

        ImGui::SetNextItemWidth(200.f);
        ImGui::InputTextWithHint("##chord", "chord name…", mChordBuf, sizeof(mChordBuf));
        ImGui::SameLine();
        if (ImGui::Button("Send Chord") && mChordBuf[0]) {
            sync->sendChord(mChordBuf);
            pushLog(std::string("Chord sent: ") + mChordBuf);
        }
        ImGui::SameLine();
        ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0.65f, 0.1f, 0.1f, 1.f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.85f, 0.2f, 0.2f, 1.f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive,  ImVec4(1.00f, 0.0f, 0.0f, 1.f));
        if (ImGui::Button("PANIC All")) {
            sync->sendPanic();
            pushLog("PANIC sent to all slaves.");
        }
        ImGui::PopStyleColor(3);

        ImGui::SameLine();
        if (ImGui::Button("Sync All")) {
            sync->sendSnapshot("{}");
            pushLog("Snapshot sent to all slaves.");
        }
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Send current DAG snapshot to all slaves");
    }
}

// ── Peer table ────────────────────────────────────────────────────────────────

void NetworkPanel::renderPeerTable(SyncManager* sync) {
    ImGui::SeparatorText("Peers");

    const auto& peers = sync->getPeers();
    if (peers.empty()) {
        ImGui::TextDisabled("  No peers discovered.");
        mSelectedPeerIdx = -1;
        return;
    }

    ImGui::PushStyleColor(ImGuiCol_TableBorderLight, ImVec4(0.3f, 0.3f, 0.3f, 1.f));
    if (ImGui::BeginTable("##peers", 6,
            ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
            ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_ScrollY,
            ImVec2(0.f, 110.f))) {
        ImGui::TableSetupColumn("Hostname",  ImGuiTableColumnFlags_WidthStretch, 2.f);
        ImGui::TableSetupColumn("IP",        ImGuiTableColumnFlags_WidthStretch, 2.f);
        ImGui::TableSetupColumn("Role",      ImGuiTableColumnFlags_WidthFixed,   65.f);
        ImGui::TableSetupColumn("Latency",   ImGuiTableColumnFlags_WidthFixed,   60.f);
        ImGui::TableSetupColumn("Uptime",    ImGuiTableColumnFlags_WidthFixed,   55.f);
        ImGui::TableSetupColumn("Status",    ImGuiTableColumnFlags_WidthFixed,   50.f);
        ImGui::TableHeadersRow();

        for (int i = 0; i < static_cast<int>(peers.size()); ++i) {
            const auto& p = peers[i];
            ImGui::TableNextRow();

            bool selected = (mSelectedPeerIdx == i);
            ImGui::TableSetColumnIndex(0);
            if (ImGui::Selectable(p.hostname.c_str(), selected,
                    ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowOverlap,
                    ImVec2(0, 0))) {
                mSelectedPeerIdx = (selected ? -1 : i);
            }

            ImGui::TableSetColumnIndex(1);
            ImGui::TextUnformatted(p.ip.c_str());

            ImGui::TableSetColumnIndex(2);
            ImGui::TextUnformatted(syncRoleName(p.role));

            ImGui::TableSetColumnIndex(3);
            if (p.latencyMs > 0.f) {
                ImVec4 col = p.latencyMs < 50.f
                    ? ImVec4(0.2f, 1.f, 0.2f, 1.f)   // green
                    : ImVec4(1.f, 0.8f, 0.f, 1.f);   // yellow if >50ms
                ImGui::TextColored(col, "%.1f ms", p.latencyMs);
            } else {
                ImGui::TextDisabled("—");
            }

            ImGui::TableSetColumnIndex(4);
            if (p.startTimeMs > 0) {
                long long sec = p.startTimeMs / 1000;
                ImGui::Text("%llds", sec);
            } else {
                ImGui::TextDisabled("—");
            }

            ImGui::TableSetColumnIndex(5);
            if (p.connected)
                ImGui::TextColored(ImVec4(0.2f, 1.f, 0.2f, 1.f), "on");
            else
                ImGui::TextColored(ImVec4(1.f, 0.3f, 0.3f, 1.f), "off");
        }
        ImGui::EndTable();
    }
    ImGui::PopStyleColor();

    // Keep selection valid if peers shrink
    if (mSelectedPeerIdx >= static_cast<int>(peers.size()))
        mSelectedPeerIdx = -1;
}

// ── Peer detail ───────────────────────────────────────────────────────────────

void NetworkPanel::renderPeerDetail(SyncManager* sync) {
    const auto& peers = sync->getPeers();
    if (mSelectedPeerIdx < 0 || mSelectedPeerIdx >= static_cast<int>(peers.size())) return;

    const auto& p = peers[mSelectedPeerIdx];

    ImGui::SeparatorText(("Detail: " + p.hostname).c_str());

    ImGui::Text("Hostname : %s", p.hostname.c_str());
    ImGui::Text("IP       : %s", p.ip.c_str());
    ImGui::Text("Role     : %s", syncRoleName(p.role));
    ImGui::Text("TCP ID   : %d", p.tcpClientId);
    ImGui::Text("Status   : %s", p.connected ? "connected" : "disconnected");
    if (p.latencyMs > 0.f) ImGui::Text("Latency  : %.2f ms", p.latencyMs);

    // Latency graph
    auto* hist = findOrCreateHistory(p.ip);
    if (!hist->samples.empty()) {
        std::vector<float> buf(hist->samples.begin(), hist->samples.end());
        float maxV = *std::max_element(buf.begin(), buf.end());
        float avgV = std::accumulate(buf.begin(), buf.end(), 0.f) / buf.size();
        char overlay[32];
        snprintf(overlay, sizeof(overlay), "avg %.1f ms", avgV);
        ImGui::PlotLines("Latency##graph", buf.data(), static_cast<int>(buf.size()),
                         0, overlay, 0.f, maxV + 5.f, ImVec2(0.f, 48.f));
    }

    // Disconnect button
    if (ImGui::SmallButton("Disconnect") && sync->isRunning()) {
        sync->removePeer(p.ip);
        pushLog("Disconnected from " + p.hostname);
    }
}

// ── Connection log ────────────────────────────────────────────────────────────

void NetworkPanel::renderLog() {
    ImGui::SeparatorText("Log");
    ImGui::BeginChild("##netlog", ImVec2(0.f, 110.f), false, ImGuiWindowFlags_HorizontalScrollbar);

    for (const auto& entry : mLog) {
        // Format elapsed time as [HH:MM:SS]
        auto age = std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::steady_clock::now() - entry.ts).count();
        ImGui::TextDisabled("[-%llds]", age);
        ImGui::SameLine();
        ImGui::TextUnformatted(entry.text.c_str());
    }
    // Auto-scroll to bottom
    if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY())
        ImGui::SetScrollHereY(1.f);

    ImGui::EndChild();
}

} // namespace bbfx
