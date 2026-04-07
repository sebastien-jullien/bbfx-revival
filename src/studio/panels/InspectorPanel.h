#pragma once
#include <string>
#include <functional>
#include <sol/forward.hpp>

namespace bbfx {

/// Shows the properties of the node selected in the NodeEditorPanel.
/// Provides sliders, dropdowns, inline Lua editor, and rename/delete actions.
class InspectorPanel {
public:
    explicit InspectorPanel(sol::state& lua);

    void render();
    void setSelectedNode(const std::string& name) { mSelectedNode = name; }

    /// Callback for "Add to Timeline" context menu on float params
    using AddToTimelineCb = std::function<void(const std::string& nodeName,
        const std::string& portName, float minVal, float maxVal)>;
    void setAddToTimelineCallback(AddToTimelineCb cb) { mAddToTimelineCb = std::move(cb); }

    /// Callback for recording slider changes to automation
    using RecordValueCb = std::function<void(const std::string& nodeName,
        const std::string& portName, float value, float beat)>;
    void setRecordValueCallback(RecordValueCb cb) { mRecordValueCb = std::move(cb); }
    void setRecordingState(bool rec, float beat) { mIsRecording = rec; mCurrentBeat = beat; }

private:
    void renderParamSpec();
    void renderFloatPorts();
    void renderEnumPorts();
    void renderLuaEditor();
    void renderShaderUniforms();
    void renderRenameDelete();

    sol::state& mLua;
    std::string mSelectedNode;
    char mLuaSourceBuf[8192] = {};
    bool mLuaModified = false;
    std::string mLuaError;

    struct CoalescingState {
        bool active = false;
        std::string nodeName, portName;
        float oldValue = 0.0f;
    };
    CoalescingState mCoalescing;
    AddToTimelineCb mAddToTimelineCb;
    RecordValueCb mRecordValueCb;
    bool mIsRecording = false;
    float mCurrentBeat = 0.0f;

    // Asset pickers (v3.2.4)
    class TextureThumbnailCache* mThumbCache = nullptr;
    char mPickerSearch[128] = {};
    std::string mPreviewOriginalTexture;
    bool mPreviewActive = false;
    std::string mPreviewCurrentTexture;

    // Fader learn mode callback (v3.2.4)
    using LearnCb = std::function<void(const std::string& nodeName, const std::string& portName)>;
    LearnCb mLearnCb;

public:
    void setThumbCache(class TextureThumbnailCache* cache) { mThumbCache = cache; }
    void setLearnCallback(LearnCb cb) { mLearnCb = std::move(cb); }
};

} // namespace bbfx
