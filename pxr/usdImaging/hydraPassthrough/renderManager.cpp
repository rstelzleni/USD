#include "renderManager.h"

#include "pxr/usdImaging/hydraPassthrough/renderDelegate.h"
#include "pxr/usdImaging/usdImaging/stageSceneIndex.h"

#include "pxr/imaging/hd/rendererPluginRegistry.h"
#include "pxr/imaging/hdsi/legacyDisplayStyleOverrideSceneIndex.h"



PXR_NAMESPACE_OPEN_SCOPE

HdPassthroughRenderManager::~HdPassthroughRenderManager() {
    Cleanup();
}

void HdPassthroughRenderManager::Initialize() {
    // This is basically blind data for our use
    _driver = {TfToken("HdPassthroughDriver"), VtValue()};

    // Scene delegate ID is used to indentify our scene index. So, all
    // paths in the renderer will be preceded with this.
    _sceneDelegateId = SdfPath("/HdPassthroughSceneDelegate");

    if (!_SetRendererPlugin(TfToken("HydraPassthroughRendererPlugin"))) {
        TF_CODING_ERROR("Failed to set renderer plugin.");
        return;
    }
    _engine = std::make_unique<HdEngine>();
}

void HdPassthroughRenderManager::Render(const UsdStageRefPtr& stage) {
    if (!stage) {
        TF_CODING_ERROR("No stage provided for rendering.");
        return;
    }

    if (!_engine || !_renderIndex || !_sceneIndex || !_stageSceneIndex) {
        TF_CODING_ERROR("Render engine or index not initialized.");
        return;
    }

    TF_PY_ALLOW_THREADS_IN_SCOPE();

    // Set up the scene index with the stage
    _stageSceneIndex->SetStage(stage);

    // Set up the render tasks
    const SdfPath taskId = SdfPath("/HdPassthroughRenderManager/RenderTask");
    _taskControllerSceneIndex = HdxTaskControllerSceneIndex::New(
        taskId,
        _renderDelegate.GetPluginId(),
        [](const TfToken &name) {
            //if (name == HdAovTokens->color) {
                // There seems to be no way to do this without allocating a buffer,
                // so aim for a small one. This might be worth investigating further.
                //
                // Note this could also include a clear color, and a settings map.
            //    return HdAovDescriptor(HdFormatUNorm8, /*multisample*/false, {});
            //}
            return HdAovDescriptor();
        },
        false // gpuEnabled
    );
    if (!_taskControllerSceneIndex) {
        TF_CODING_ERROR("Failed to create task controller scene index.");
        return;
    }
    _renderIndex->InsertSceneIndex(
        _taskControllerSceneIndex,
        taskId,
        /* needsPrefixing = */ false);

    // Set the render collection
    // subdivide by default, for now, I guess.
    HdReprSelector reprSelector = HdReprSelector(HdReprTokens->refined);
    const TfToken collectionName = HdTokens->geometry;
    // This should probably be persistent, it's expensive to update
    HdRprimCollection collection = HdRprimCollection(collectionName, reprSelector);
    // Yikes
    const SdfPathVector paths = {
        _sceneDelegateId, // This is SdfPath::AbsoluteRootPath() in this context
    };
    collection.SetRootPaths(paths);
    _taskControllerSceneIndex->SetCollection(collection);

    // _PrepareRender
    // Tags can include guides, proxies, etc.
    TfTokenVector renderTags{
        HdRenderTagTokens->geometry
    };
    //_taskControllerSceneIndex->SetFreeCameraClipPlanes(params.clipPlanes);
    _taskControllerSceneIndex->SetRenderTags(renderTags);
    // Includes things like culling style, override color, camera lights, etc.
    // Use defaults for now
    HdxRenderTaskParams params;
    _taskControllerSceneIndex->SetRenderParams(params);
    // Prepare render here also uses some extra scene indices for things like cull
    // style, pruning lights and materials.

    // Render the frame using the engine
    _engine->Execute(_renderIndex.get(), _taskControllerSceneIndex->GetRenderingTaskPaths());
}

void HdPassthroughRenderManager::Cleanup()
{
    // Destroy objects in opposite order of construction.
    _engine = nullptr;

    if (_renderIndex && _sceneIndex) {
        _renderIndex->RemoveSceneIndex(_sceneIndex);
        _stageSceneIndex = nullptr;
        _sceneIndex = nullptr;
    }

    _renderIndex = nullptr;
    _renderDelegate = nullptr;
}

HydraPassthroughRenderDataRefPtr HdPassthroughRenderManager::GetRenderData() const {
    auto rd = dynamic_cast<HydraPassthroughRenderDelegate*>(_renderDelegate.Get());
    if (!rd) {
        TF_CODING_ERROR("Render delegate is not a HydraPassthroughRenderDelegate.");
        return nullptr;
    }
    return rd->GetRenderData();
}

bool HdPassthroughRenderManager::_SetRendererPlugin(const TfToken& id) {
    HdRendererPluginRegistry &registry =
        HdRendererPluginRegistry::GetInstance();

    // UsdImagingGLEngine does this, what is it for?
     TF_PY_ALLOW_THREADS_IN_SCOPE();

    _renderDelegate = registry.CreateRenderDelegate(id);
    if (!_renderDelegate) {
        return false;
    }

    // If a delegate exists already we need to destroy hydra memory here

    // Need a unique id string
    const std::string renderInstanceId =
        TfStringPrintf("HdPassthroughRenderManager_%s_%p",
            _renderDelegate.GetPluginId().GetText(),
            (void *) _renderDelegate.Get());

    // Set up the scene globals scene index, SGSI

    // Recreate the render index
    _renderIndex.reset(
        HdRenderIndex::New(
            _renderDelegate.Get(), {&_driver}, renderInstanceId));

    // Let the output render data know what prefix to expect on SdfPaths
    GetRenderData()->SetSceneDelegateId(_sceneDelegateId);

    // Set up scene indices. We could provide a stage in this info object
    //
    // Note that this supports selection and things we don't need. Also, this
    // is where we have a dependency on UsdImaging. If we wrote our own scene
    // indices we could avoid this.
    UsdImagingCreateSceneIndicesInfo info;
    const UsdImagingSceneIndices sceneIndices =
        UsdImagingCreateSceneIndices(info);

    _stageSceneIndex = sceneIndices.stageSceneIndex;

    _sceneIndex = 
        HdsiLegacyDisplayStyleOverrideSceneIndex::New(sceneIndices.finalSceneIndex);

    _renderIndex->InsertSceneIndex(_sceneIndex, _sceneDelegateId);

    return true;
}

PXR_NAMESPACE_CLOSE_SCOPE
