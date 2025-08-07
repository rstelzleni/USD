#ifndef USD_IMAGING_HYDRA_PASSTHROUGH_RENDER_PASS_H
#define USD_IMAGING_HYDRA_PASSTHROUGH_RENDER_PASS_H

#include "pxr/imaging/hd/renderPass.h"
#include "pxr/pxr.h"

PXR_NAMESPACE_OPEN_SCOPE

/// \class HydraPassthroughRenderPass
///
/// HdRenderPass represents a single render iteration, rendering a view of the
/// scene (the HdRprimCollection) for a specific viewer (the camera/viewport
/// parameters in HdRenderPassState) to the current draw target.
///
/// In this implementation this will be more or less a no-op. Passthrough means
/// bypassing the render, just prepping the data.
class HydraPassthroughRenderPass final : public HdRenderPass {
public:
    /// Renderpass constructor.
    ///   \param index The render index containing scene data to render.
    ///   \param collection The initial rprim collection for this renderpass.
    HydraPassthroughRenderPass(HdRenderIndex *index, HdRprimCollection const &collection);

    /// Renderpass destructor.
    virtual ~HydraPassthroughRenderPass();

protected:
    /// Draw the scene with the bound renderpass state.
    ///   \param renderPassState Input parameters (including viewer parameters)
    ///                          for this renderpass.
    ///   \param renderTags Which rendertags should be drawn this pass.
    void _Execute(HdRenderPassStateSharedPtr const &renderPassState,
                  TfTokenVector const &renderTags) override;
};

PXR_NAMESPACE_CLOSE_SCOPE

#endif // USD_IMAGING_HYDRA_PASSTHROUGH_RENDER_PASS_H
