#include "renderManager.h"

#include "pxr/imaging/hd/rendererPluginRegistry.h"
#include "pxr/imaging/hdsi/legacyDisplayStyleOverrideSceneIndex.h"


PXR_NAMESPACE_OPEN_SCOPE

HdPassthroughRenderManager::~HdPassthroughRenderManager() {
    Cleanup();
}

void HdPassthroughRenderManager::Initialize() {
    // This is basically blind data for our use
    _driver = {TfToken("HdPassthroughDriver"), VtValue()};

    // Scene delegate ID is used for what?
    _sceneDelegateId = SdfPath("/HdPassthroughSceneDelegate");

    if (!_SetRendererPlugin(TfToken("HdTinyRendererPlugin"))) {
        TF_CODING_ERROR("Failed to set renderer plugin.");
        return;
    }
    _engine = std::make_unique<HdEngine>();
}

void HdPassthroughRenderManager::RenderFrame() {
    // Render a frame
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

bool HdPassthroughRenderManager::_SetRendererPlugin(const TfToken& id) {
    HdRendererPluginRegistry &registry =
        HdRendererPluginRegistry::GetInstance();

    // UsdImagingGLEngine does this, what is it for?
     TF_PY_ALLOW_THREADS_IN_SCOPE();

    _renderDelegate = registry.CreateRenderDelegate(id);
    if (!_renderDelegate) {
        return false;
    }

//    _SetRenderDelegateAndRestoreState(std::move(renderDelegate));

    // Could restore state for new plugin here, but we'll just init for now
    //GfMatrix4d rootTransform = GfMatrix4d(1.0);
    //bool rootVisibility = true;

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

    // Set up scene indices. We could provide a stage in this info object
    //
    // Note that this supports selection and things we don't need
    UsdImagingCreateSceneIndicesInfo info;
    const UsdImagingSceneIndices sceneIndices =
        UsdImagingCreateSceneIndices(info);

    _stageSceneIndex = sceneIndices.stageSceneIndex;

    _sceneIndex = 
        HdsiLegacyDisplayStyleOverrideSceneIndex::New(sceneIndices.finalSceneIndex);

    _renderIndex->InsertSceneIndex(_sceneIndex, _sceneDelegateId);

    /*
    if (_allowAsynchronousSceneProcessing) {
        if (HdSceneIndexBaseRefPtr si = _renderIndex->GetTerminalSceneIndex()) {
            si->SystemMessage(HdSystemMessageTokens->asyncAllow, nullptr);
        }
    }
    */

    /*
    if (_GetUseTaskControllerSceneIndex()) {
        const SdfPath taskControllerPath =
            _ComputeControllerPath(_renderDelegate);
        _taskControllerSceneIndex = HdxTaskControllerSceneIndex::New(
            taskControllerPath,
            renderDelegate.GetPluginId(),
            [renderDelegate = _renderDelegate.Get()](const TfToken &name) {
                return renderDelegate->GetDefaultAovDescriptor(name); },
            _gpuEnabled);
        _renderIndex->InsertSceneIndex(
            _taskControllerSceneIndex,
            taskControllerPath,
            false); // needsPrefixing 
    } else {
        _taskController = std::make_unique<HdxTaskController>(
            _renderIndex.get(),
            _ComputeControllerPath(_renderDelegate),
            _gpuEnabled);
    }
    */

    return true;
}

PXR_NAMESPACE_CLOSE_SCOPE
