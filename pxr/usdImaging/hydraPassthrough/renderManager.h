#pragma once

#include "pxr/pxr.h"

#include "pxr/usdImaging/usdImaging/sceneIndices.h"

#include "pxr/imaging/hd/driver.h"
#include "pxr/imaging/hd/engine.h"
#include "pxr/imaging/hd/pluginRenderDelegateUniqueHandle.h"
#include "pxr/imaging/hdx/taskControllerSceneIndex.h"

#include "pxr/usd/sdf/path.h"
#include "pxr/usd/usd/stage.h"

#include "pxr/base/tf/declarePtrs.h"

PXR_NAMESPACE_OPEN_SCOPE

TF_DECLARE_REF_PTRS(HydraPassthroughRenderData);

class HdPassthroughRenderManager {
public:
    HdPassthroughRenderManager() = default;
    ~HdPassthroughRenderManager();

    // We could add a repr description parameter, but we're missing some of the
    // machinery from HdSt to make that work. Currently we are always using
    // HdReprTokens->refined. To see what needs to be done to add more reprs,
    // see HdRenderIndex::_ConfigureReprs() for what each repr token sets, then
    // search in HdSt to see where these values are checked.
    //
    // If we want to enable flat normals, unsubdivided meshes, wireframe, points,
    // etc. this is the place to do it.

    struct RenderSettings {
        RenderSettings() : refineLevel(1) {}
        int refineLevel;
    };

    // Initialize the render manager
    void Initialize(const RenderSettings& settings = RenderSettings());

    // Render a frame
    void Render(const UsdStageRefPtr& stage);

    // Cleanup resources
    void Cleanup();

    HydraPassthroughRenderDataRefPtr GetRenderData() const;

    static SdfPath GetSceneDelegateId();

private:

    bool _SetRendererPlugin(const TfToken& id);

    SdfPath _sceneDelegateId;
    UsdImagingStageSceneIndexRefPtr _stageSceneIndex;
    HdSceneIndexBaseRefPtr _sceneIndex;
    HdxTaskControllerSceneIndexRefPtr _taskControllerSceneIndex;
    HdDriver _driver;
    std::unique_ptr<HdRenderIndex> _renderIndex;
    HdPluginRenderDelegateUniqueHandle _renderDelegate;

    // There is one engine per render manager. Engines hold no shared state.
    // The only constraint I can see with multiple engines is that only one can
    // Execute() against a single render index at a time. Since each manager
    // has its own render index and delegate, this does not need any locking
    // and clients don't need to be careful about multiple render managers 
    std::unique_ptr<HdEngine> _engine;
};

PXR_NAMESPACE_CLOSE_SCOPE
