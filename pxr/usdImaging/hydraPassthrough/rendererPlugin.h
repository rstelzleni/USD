#ifndef USD_IMAGING_HYDRA_PASSTHROUGH_RENDERER_PLUGIN_H
#define USD_IMAGING_HYDRA_PASSTHROUGH_RENDERER_PLUGIN_H

#include "pxr/imaging/hd/rendererPlugin.h"
#include "pxr/pxr.h"

PXR_NAMESPACE_OPEN_SCOPE

///
/// \class HydraPassthroughRendererPlugin
///
/// A registered child of HdRendererPlugin, this is the class that gets
/// loaded when a Hydra application asks to draw with a certain renderer.
/// It supports rendering via creation/destruction of renderer-specific
/// classes. The render delegate is the Hydra-facing entrypoint into the
/// renderer; it's responsible for creating specialized implementations of Hydra
/// prims (which translate scene data into drawable representations) and Hydra
/// renderpasses (which draw the scene to the framebuffer).
///
class HydraPassthroughRendererPlugin final : public HdRendererPlugin {
public:
    HydraPassthroughRendererPlugin() = default;
    virtual ~HydraPassthroughRendererPlugin() = default;

    /// Construct a new render delegate of type HydraPassthroughRenderDelegate.
    virtual HdRenderDelegate *CreateRenderDelegate() override;

    /// Construct a new render delegate of type HydraPassthroughRenderDelegate.
    virtual HdRenderDelegate *
        CreateRenderDelegate(HdRenderSettingsMap const &settingsMap) override;

    /// Destroy a render delegate created by this class's CreateRenderDelegate.
    ///   \param renderDelegate The render delegate to delete.
    virtual void DeleteRenderDelegate(HdRenderDelegate *renderDelegate) override;

    /// Checks to see if the plugin is supported on the running system.
    virtual bool IsSupported(bool gpuEnabled = true) const override;

private:
    // This class does not support copying.
    HydraPassthroughRendererPlugin(const HydraPassthroughRendererPlugin &) = delete;
    HydraPassthroughRendererPlugin &operator=(const HydraPassthroughRendererPlugin &) = delete;
};

PXR_NAMESPACE_CLOSE_SCOPE

#endif // USD_IMAGING_HYDRA_PASSTHROUGH_RENDERER_PLUGIN_H
