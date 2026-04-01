#pragma once

#include <imgui.h>
#include <imgui_node_editor.h>
#include <string>
#include <functional>
#include <map>
#include <vector>

#include <sol/forward.hpp>

namespace ax::NodeEditor { struct EditorContext; }

namespace bbfx {

class AnimationNode;

/// Displays the Animator DAG as an interactive node editor (imgui-node-editor).
/// Syncs bidirectionally with the Animator singleton: REPL and GUI share the same DAG.
class NodeEditorPanel {
public:
    explicit NodeEditorPanel(sol::state& lua);
    ~NodeEditorPanel();

    void render();

    /// Callback invoked when the user selects a node. Parameter: node name.
    void setSelectionCallback(std::function<void(const std::string&)> cb) {
        mSelectionCallback = std::move(cb);
    }

    /// Returns the name of the currently selected node, or "".
    const std::string& getSelectedNodeName() const { return mSelectedNode; }

    /// Programmatically select a node (triggers selection callback for Inspector)
    void selectNode(const std::string& name) {
        mSelectedNode = name;
        if (mSelectionCallback) mSelectionCallback(name);
    }

    struct NodePosition { std::string name; float x, y; };
    std::vector<NodePosition> getNodePositions() const;
    void setNodePositions(const std::vector<NodePosition>& positions);

    /// Re-sync node/link data from the Animator DAG. Call after deferred deletes.
    void syncFromDAG();

private:
    void syncLinksFromDAG();
    void handleLinkCreation();
    void handleDeletion(); // handles both link and node deletion in one BeginDelete scope
    void showNodeContextMenu();

    /// Find the PinId for a given node name, port name and direction.
    ax::NodeEditor::PinId findPin(const std::string& nodeName,
                                   const std::string& portName,
                                   bool output) const;

    ImVec4 nodeColor(const std::string& typeName) const;

    ax::NodeEditor::EditorContext* mEditorContext = nullptr;

    struct NodeData {
        std::string name;
        std::string typeName;
        ax::NodeEditor::NodeId id;
        std::vector<ax::NodeEditor::PinId> inputPins;
        std::vector<ax::NodeEditor::PinId> outputPins;
        std::vector<std::string> inputNames;
        std::vector<std::string> outputNames;
    };

    struct LinkData {
        ax::NodeEditor::LinkId id;
        ax::NodeEditor::PinId  startPin;
        ax::NodeEditor::PinId  endPin;
        std::string fromNode, fromPort, toNode, toPort;
    };

    std::map<std::string, NodeData> mNodes;    // key = node name
    std::vector<LinkData> mLinks;

    // ID counters (imgui-node-editor needs unique integer IDs)
    int mNextId = 1;
    int allocId() { return mNextId++; }

    sol::state& mLua;

    std::string mSelectedNode;
    std::function<void(const std::string&)> mSelectionCallback;

    bool mShowCreateMenu = false;
    ImVec2 mCreateMenuPos;
    ax::NodeEditor::LinkId mContextLinkId;  // for right-click link context menu

    bool mShowSavePresetDialog = false;
    char mPresetNameBuf[128] = {};

    std::vector<NodePosition> mPendingPositions;

    // Deferred drop position (screen coords, converted to canvas in next ned::Begin scope)
    ImVec2 mDropScreenPos = {0, 0};
    std::string mDropPresetName;

    // Stale IDs from deferred-deleted nodes — cleaned up in ned after Begin()
    std::vector<ax::NodeEditor::NodeId> mStaleNodeIds;
    std::vector<ax::NodeEditor::PinId> mStalePinIds;

    ImVec2 mBookmarks[9] = {};
    float mBookmarkZooms[9] = {};
    float mBookmarkRects[9][4] = {}; // {minX, minY, maxX, maxY}
    bool mBookmarkSet[9] = {};
};

} // namespace bbfx
