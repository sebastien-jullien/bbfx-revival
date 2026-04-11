#include "NdiOutputNode.h"
#include "../../core/AnimationPort.h"
#include <iostream>

#ifdef BBFX_HAS_NDI
#include <Processing.NDI.Lib.h>
#endif

namespace bbfx {

NdiOutputNode::NdiOutputNode(const std::string& name) : AnimationNode(name) {
    addInput(new AnimationPort("enabled", 1.0f));
    addInput(new AnimationPort("dt", 0.016f));

    { ParamDef d; d.name = "source_name"; d.type = ParamType::STRING; d.stringVal = "BBFx Output"; d.label = "NDI Source Name"; mSpec.addParam(d); }
    { ParamDef d; d.name = "width"; d.type = ParamType::INT; d.intVal = 1920; d.minVal = 320; d.maxVal = 3840; d.label = "Width"; mSpec.addParam(d); }
    { ParamDef d; d.name = "height"; d.type = ParamType::INT; d.intVal = 1080; d.minVal = 240; d.maxVal = 2160; d.label = "Height"; mSpec.addParam(d); }
    { ParamDef d; d.name = "fps"; d.type = ParamType::INT; d.intVal = 30; d.minVal = 15; d.maxVal = 60; d.label = "FPS"; mSpec.addParam(d); }

    setParamSpec(&mSpec);

#ifdef BBFX_HAS_NDI
    if (NDIlib_initialize()) {
        NDIlib_send_create_t desc;
        auto* nameParam = mSpec.getParam("source_name");
        std::string srcName = nameParam ? nameParam->stringVal : "BBFx Output";
        desc.p_ndi_name = srcName.c_str();
        desc.clock_video = true;
        mNdiSender = NDIlib_send_create(&desc);
        if (mNdiSender) {
            mInitialized = true;
            std::cout << "[NDI] Sender initialized: " << srcName << std::endl;
        }
    }
#else
    std::cout << "[NDI] SDK not available — node registered but inactive. "
              << "Install NDI SDK and rebuild with -DBBFX_HAS_NDI=ON" << std::endl;
#endif
}

NdiOutputNode::~NdiOutputNode() {
#ifdef BBFX_HAS_NDI
    if (mNdiSender) {
        NDIlib_send_destroy(static_cast<NDIlib_send_instance_t>(mNdiSender));
        mNdiSender = nullptr;
    }
    NDIlib_destroy();
#endif
}

void NdiOutputNode::update() {
    if (!mInitialized) return;

    auto& ins = getInputs();
    float enabled = ins["enabled"]->getValue();
    if (enabled < 0.5f) return;

#ifdef BBFX_HAS_NDI
    // In a full implementation, this would:
    // 1. glReadPixels from the render target
    // 2. Convert to NDI frame format (BGRA or UYVY)
    // 3. NDIlib_send_send_video_v2(mNdiSender, &frame)
    //
    // The pixel readback requires access to the StudioEngine's GL context.
    // This is done via a callback or by reading from the output RenderTexture.
    mFrameCount++;
#endif

    fireUpdate();
}

} // namespace bbfx
