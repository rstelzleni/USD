#include "unboundMaterialPruningSceneIndexPlugin.h"

#include "pxr/imaging/hd/materialBindingsSchema.h"
#include "pxr/imaging/hd/retainedDataSource.h"
#include "pxr/imaging/hd/sceneIndexPluginRegistry.h"
#include "pxr/imaging/hd/tokens.h"
#include "pxr/imaging/hdsi/unboundMaterialPruningSceneIndex.h"

PXR_NAMESPACE_OPEN_SCOPE

TF_DEFINE_PRIVATE_TOKENS(
    _tokens,
    ((sceneIndexPluginName,
      "HydraPassthrough_UnboundMaterialPruningSceneIndexPlugin"))
);

// Must match the renderer plugin's displayName in plugInfo.json
static const char * const _rendererDisplayName =
    "Hydra Passthrough Renderer Plugin";

TF_REGISTRY_FUNCTION(TfType)
{
    HdSceneIndexPluginRegistry::Define<
        HydraPassthrough_UnboundMaterialPruningSceneIndexPlugin>();
}

TF_REGISTRY_FUNCTION(HdSceneIndexPlugin)
{
    // Same phase as Storm: as far downstream as possible, so bindings
    // added by earlier scene indices are seen.
    const HdSceneIndexPluginRegistry::InsertionPhase insertionPhase = 900;

    HdSceneIndexPluginRegistry::GetInstance().RegisterSceneIndexForRenderer(
        _rendererDisplayName,
        _tokens->sceneIndexPluginName,
        /* inputArgs = */ nullptr,
        insertionPhase,
        HdSceneIndexPluginRegistry::InsertionOrderAtStart);
}

HydraPassthrough_UnboundMaterialPruningSceneIndexPlugin::
HydraPassthrough_UnboundMaterialPruningSceneIndexPlugin() = default;

HdSceneIndexBaseRefPtr
HydraPassthrough_UnboundMaterialPruningSceneIndexPlugin::_AppendSceneIndex(
    const HdSceneIndexBaseRefPtr &inputScene,
    const HdContainerDataSourceHandle &inputArgs)
{
    // The binding purposes we resolve, in the same order as Storm.
    static const HdTokenArrayDataSourceHandle bindingPurposesDs =
        HdRetainedTypedSampledDataSource<VtArray<TfToken>>::New(
            VtArray<TfToken>({
                HdTokens->preview,
                HdMaterialBindingsSchemaTokens->allPurpose,
            }));

    static const HdContainerDataSourceHandle localInputArgs =
        HdRetainedContainerDataSource::New(
            HdsiUnboundMaterialPruningSceneIndexTokens->materialBindingPurposes,
            bindingPurposesDs);

    return HdsiUnboundMaterialPruningSceneIndex::New(
        inputScene, localInputArgs);
}

PXR_NAMESPACE_CLOSE_SCOPE
