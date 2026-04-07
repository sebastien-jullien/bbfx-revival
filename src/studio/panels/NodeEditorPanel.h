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

    /// Callback for creating nodes from drops (texture, material) with optional entity link.
    using CreateNodeFn = std::function<void(const std::string& nodeType, const std::string& paramValue,
                                             const std::string& targetNode)>;
    void setCreateNodeCallback(CreateNodeFn fn) { mCreateNodeFn = std::move(fn); }

    /// Programmatically select a node (triggers selection callback for Inspector)
    void selectNode(const std::string& name) {
        mSelectedNode = name;
        if (mSelectionCallback) mSelectionCallback(name);
    }

    /// Select a node from external source (viewport picking) WITHOUT firing the callback.
    /// This avoids infinite selection loops.
    void selectNodeFromExternal(const std::string& name) {
        mSelectedNode = name;
    }

    struct NodePosition { std::string name; float x, y; int attempts = 0; };
    std::vector<NodePosition> getNodePositions() const;
    void setNodePositions(const std::vector<NodePosition>& positions);

    /// Position a newly created node next to a target node (for viewport drops)
    void positionNodeNextTo(const std::string& newNodeName, const std::string& targetNodeName);

    /// Get the last drop screen position (set during drag-drop accept)
    ImVec2 getDropScreenPos() const { return mDropScreenPos; }

    /// Schedule positioning of a dropped asset node (texture/material) at the drop screen position
    void scheduleDropPosition(const std::string& nodeName, const std::string& target, ImVec2 screenPos) {
        mDropAssetNodeName = nodeName;
        mDropAssetTarget = target;
        mDropAssetScreenPos = screenPos;
    }

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
    CreateNodeFn mCreateNodeFn;

    // Cached node rects + mouse canvas pos for hit-test during drag-drop
    struct NodeRect { float x, y, w, h; };
    std::map<std::string, NodeRect> mCachedNodeRects;
    // Cached screen→canvas transform
    ImVec2 mCanvasRef0 = {0, 0};
    ImVec2 mCanvasRef1 = {1, 0};
    ImVec2 screenToCanvasCached(ImVec2 sp) const {
        float sx = mCanvasRef1.x - mCanvasRef0.x; // scale per screen pixel
        return {mCanvasRef0.x + sp.x * sx, mCanvasRef0.y + sp.y * sx};
    }

    // Deferred drop positioning for texture/material nodes (same pattern as mDropPresetName)
    std::string mDropAssetNodeName;  // name of the TextureNode/MaterialNode to position
    std::string mDropAssetTarget;    // SceneObjectNode target (for positioning next to it)
    ImVec2 mDropAssetScreenPos = {0, 0};

    bool mShowCreateMenu = false;
    ImVec2 mCreateMenuPos;
    ax::NodeEditor::LinkId mContextLinkId;  // for right-click link context menu

    bool mShowSavePresetDialog = false;
    char mPresetNameBuf[128] = {};

    std::vector<NodePosition> mPendingPositions;
    std::vector<std::string> mNewNodes;  // nodes added in current syncFromDAG call

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
