#pragma once

#include <imgui.h>
#include <imgui_node_editor.h>
#include <string>
#include <functional>
#include <map>
#include <vector>
#include <set>

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

    /// M10 — sérialise un node en preset Lua (params réels). Extrait du modal
    /// « Save as Preset » pour être testable. Retourne true si le fichier a été écrit.
    bool savePreset(const std::string& presetName, const std::string& nodeName);

    /// Callback invoked when the user selects a node. Parameter: node name.
    void setSelectionCallback(std::function<void(const std::string&)> cb) {
        mSelectionCallback = std::move(cb);
    }

    /// Returns the name of the currently selected node (primary), or "".
    const std::string& getSelectedNodeName() const { return mSelectedNode; }

    /// Returns all currently selected node names.
    const std::set<std::string>& getSelectedNodeNames() const { return mSelectedNodes; }

    // Accessors declared after private members (see below)

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

    // ── Clipboard for copy-paste ────────────────────────────────────────────
    struct NodeSnapshot {
        std::string name, typeName;
        float relX = 0, relY = 0;        // position relative to selection centroid
        std::map<std::string, float> portValues;
        std::string paramSpecJsonStr;  // JSON string of ParamSpec
    };
    struct LinkSnapshot {
        std::string fromNode, fromPort, toNode, toPort;
    };
    struct ClipboardData {
        std::vector<NodeSnapshot> nodes;
        std::vector<LinkSnapshot> links;
    };

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

    /// v3.5.2 Sprint S6 Lot W: per-port color by name convention (entity-link / mat /
    /// tex / ready / default). Documented in USAGE.md "NodeEditor visual conventions".
    ImVec4 portColor(const std::string& portName) const;

    /// v3.5.2 Sprint S6 Lot X / AU.5: categorical column for auto-layout heuristic.
    /// 0 = RootTimeNode, 1 = other Inputs, 2 = FX, 3 = Scene, 4 = Output.
    int categoryColumn(const std::string& typeName) const;
    /// Estimated node height (px) for auto-layout before the node is drawn.
    float estimatedNodeHeight(const std::string& name) const;

public:
    /// v3.5.2 Sprint S6 Lot X: BFS topological sort + categorical clamp + spatial layout
    /// for `newNames`. Skips any node already at non-zero position (preserves user layout).
    /// Pushes positions into `mPendingPositions` (applied after `ned::End` like all others).
    /// Cycles in DAG (e.g. TextureFeedbackNode self-link) are handled via visited-set.
    /// Public depuis Sprint S7 Lot Z (test ALY-005 reel cycle DAG via Debugger).
    /// `assumeFresh` = the nodes were just created (no position yet) → skip the
    /// ned::GetNodePosition() probe (which crashes on a not-yet-rendered node id).
    /// Used by syncFromDAG() when new nodes appear from a builder/console.
    void autoLayoutNodes(const std::vector<std::string>& newNames, bool assumeFresh = false);
private:

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

    std::string mSelectedNode;                   // primary selection (first of multi-select)
    std::set<std::string> mSelectedNodes;          // all currently selected nodes
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

    // Quick-add popup state
    bool mShowQuickAdd = false;
    ImVec2 mQuickAddPos;           // canvas position for node creation
    char mQuickAddBuf[128] = {};
    int mQuickAddSelection = 0;

    // Drag-link auto-create state
    struct PendingDragLink {
        std::string nodeName, portName;
        bool isOutput = false;
        bool active = false;
    };
    PendingDragLink mPendingDragLink;
    ax::NodeEditor::LinkId mContextLinkId;  // for right-click link context menu

    bool mShowSavePresetDialog = false;
    char mPresetNameBuf[128] = {};

    ImVec2 mCreateMenuPos;  // screen position where context menu was opened
    std::vector<NodePosition> mPendingPositions;
    std::vector<std::string> mNewNodes;  // nodes added in current syncFromDAG call

    // Deferred drop position (screen coords, converted to canvas in next ned::Begin scope)
    ImVec2 mDropScreenPos = {0, 0};
    std::string mDropPresetName;

    // Stale IDs from deferred-deleted nodes — cleaned up in ned after Begin()
    std::vector<ax::NodeEditor::NodeId> mStaleNodeIds;
    std::vector<ax::NodeEditor::PinId> mStalePinIds;

    ClipboardData mClipboard;
    static int sCopyCounter;

    // Node comments (v3.2.5 — I-686)
    std::map<std::string, std::string> mNodeComments;
    bool mShowCommentPopup = false;
    char mCommentBuf[512] = {};
    std::string mCommentTarget;

    // Node groups — struct public for test automation access
    public:
    struct NodeGroup {
        std::string name;
        float hue = 0.5f;
        std::vector<std::string> memberNames;
    };
    private:
    std::vector<NodeGroup> mNodeGroups;

    // Collapsed nodes (v3.2.5 — I-689)
    std::set<std::string> mCollapsedNodes;

    // Minimap cached view rect (captured inside ned::Begin/End scope)
    float mViewRect[4] = {0, 0, 1, 1}; // {minX, minY, maxX, maxY} in canvas coords

    ImVec2 mBookmarks[9] = {};
    float mBookmarkZooms[9] = {};
    float mBookmarkRects[9][4] = {}; // {minX, minY, maxX, maxY}
    bool mBookmarkSet[9] = {};

public:
    // Accessors for test automation (v3.2.5)
    std::vector<NodeGroup>& getNodeGroups() { return mNodeGroups; }
    const std::vector<NodeGroup>& getNodeGroups() const { return mNodeGroups; }
    std::map<std::string, std::string>& getNodeComments() { return mNodeComments; }
    const std::map<std::string, std::string>& getNodeComments() const { return mNodeComments; }
    std::set<std::string>& getCollapsedNodes() { return mCollapsedNodes; }
    const std::set<std::string>& getCollapsedNodes() const { return mCollapsedNodes; }
};

} // namespace bbfx
