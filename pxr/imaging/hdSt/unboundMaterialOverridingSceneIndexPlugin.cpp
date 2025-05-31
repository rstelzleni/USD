//
// Copyright 2025 Pixar
//
// Licensed under the terms set forth in the LICENSE.txt file available at
// https://openusd.org/license.

#include "pxr/imaging/hdSt/unboundMaterialOverridingSceneIndexPlugin.h"

#include "pxr/imaging/hd/dataSourceTypeDefs.h"
#include "pxr/imaging/hd/retainedDataSource.h"
#include "pxr/imaging/hd/materialBindingsSchema.h"
#include "pxr/imaging/hd/sceneIndexPluginRegistry.h"
#include "pxr/imaging/hd/tokens.h"
#include "pxr/imaging/hdsi/unboundMaterialOverridingSceneIndex.h"

#include "pxr/base/tf/envSetting.h"

PXR_NAMESPACE_OPEN_SCOPE

// XXX Temporary env setting to disable the scene index to address performance
//     regressions.
TF_DEFINE_ENV_SETTING(HDST_ENABLE_UNBOUND_MATERIAL_OVERRIDING_SCENE_INDEX,
    false, "Enable scene index that nullifies unbound materials.");

TF_DEFINE_PRIVATE_TOKENS(
    _tokens,
    ((sceneIndexPluginName, "HdSt_UnboundMaterialOverridingSceneIndexPlugin"))
);

static const char * const _pluginDisplayName = "GL";

static bool
_IsEnabled()
{
    static bool enabled =
        TfGetEnvSetting(HDST_ENABLE_UNBOUND_MATERIAL_OVERRIDING_SCENE_INDEX);
    return enabled;
}

TF_REGISTRY_FUNCTION(TfType)
{
    HdSceneIndexPluginRegistry::Define<
        HdSt_UnboundMaterialOverridingSceneIndexPlugin>();
}

TF_REGISTRY_FUNCTION(HdSceneIndexPlugin)
{
    // This scene index should be added *before*
    // HdSt_DependencyForwardingSceneIndexPlugin (which currently uses 1000).
    const HdSceneIndexPluginRegistry::InsertionPhase insertionPhase = 900;

    const HdTokenArrayDataSourceHandle bindingPurposesDs =
        HdRetainedTypedSampledDataSource<VtArray<TfToken>>::New(
            VtArray<TfToken>({
                HdTokens->preview,
                HdMaterialBindingsSchemaTokens->allPurpose,
            }));
    
    const HdContainerDataSourceHandle inputArgs =
        HdRetainedContainerDataSource::New(
            HdsiUnboundMaterialOverridingSceneIndexTokens->
                materialBindingPurposes,
            bindingPurposesDs);

    HdSceneIndexPluginRegistry::GetInstance().RegisterSceneIndexForRenderer(
        _pluginDisplayName,
        _tokens->sceneIndexPluginName,
        inputArgs,
        insertionPhase,
        HdSceneIndexPluginRegistry::InsertionOrderAtStart);
}

HdSt_UnboundMaterialOverridingSceneIndexPlugin::
HdSt_UnboundMaterialOverridingSceneIndexPlugin() = default;

HdSceneIndexBaseRefPtr
HdSt_UnboundMaterialOverridingSceneIndexPlugin::_AppendSceneIndex(
    const HdSceneIndexBaseRefPtr &inputScene,
    const HdContainerDataSourceHandle &inputArgs)
{
    if (_IsEnabled()) {
        return HdsiUnboundMaterialOverridingSceneIndex::New(
            inputScene, inputArgs);
    }
    return inputScene;
}

PXR_NAMESPACE_CLOSE_SCOPE
