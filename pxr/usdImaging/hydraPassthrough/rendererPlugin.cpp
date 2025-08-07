#include "rendererPlugin.h"
#include "renderDelegate.h"

#include "pxr/imaging/hd/rendererPluginRegistry.h"

PXR_NAMESPACE_OPEN_SCOPE

// Register the plugin with the renderer plugin system.
TF_REGISTRY_FUNCTION(TfType) {
    HdRendererPluginRegistry::Define<HydraPassthroughRendererPlugin>();
}

HdRenderDelegate *HydraPassthroughRendererPlugin::CreateRenderDelegate() {
    return new HydraPassthroughRenderDelegate();
}

HdRenderDelegate *HydraPassthroughRendererPlugin::CreateRenderDelegate(
        HdRenderSettingsMap const &settingsMap) {
    return new HydraPassthroughRenderDelegate(settingsMap);
}

void HydraPassthroughRendererPlugin::DeleteRenderDelegate(
        HdRenderDelegate *renderDelegate) {
    delete renderDelegate;
}

bool HydraPassthroughRendererPlugin::IsSupported(bool /* gpuEnabled */) const {
    // Nothing more to check for now, we assume if the plugin loads correctly
    // it is supported.
    return true;
}

PXR_NAMESPACE_CLOSE_SCOPE
