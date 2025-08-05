#pragma once

#include "pxr/pxr.h"

#include "pxr/usdImaging/usdImaging/sceneIndices.h"

#include "pxr/imaging/hd/driver.h"
#include "pxr/imaging/hd/engine.h"
#include "pxr/imaging/hd/pluginRenderDelegateUniqueHandle.h"

#include "pxr/usd/sdf/path.h"

#include "pxr/base/tf/declarePtrs.h"

PXR_NAMESPACE_OPEN_SCOPE



class HdPassthroughRenderManager {
public:
    HdPassthroughRenderManager() = default;
    ~HdPassthroughRenderManager();

    // Initialize the render manager
    void Initialize();

    // Render a frame
    void RenderFrame();

    // Cleanup resources
    void Cleanup();

private:

    bool _SetRendererPlugin(const TfToken& id);

    SdfPath _sceneDelegateId;
    UsdImagingStageSceneIndexRefPtr _stageSceneIndex;
    HdSceneIndexBaseRefPtr _sceneIndex;
    HdDriver _driver;
    std::unique_ptr<HdRenderIndex> _renderIndex;
    HdPluginRenderDelegateUniqueHandle _renderDelegate;
    std::unique_ptr<HdEngine> _engine;
};

PXR_NAMESPACE_CLOSE_SCOPE
