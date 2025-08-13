#ifndef USD_IMAGING_HYDRA_PASSTHROUGH_RENDER_PASS_H
#define USD_IMAGING_HYDRA_PASSTHROUGH_RENDER_PASS_H

#include "pxr/pxr.h"
#include "pxr/usdImaging/hydraPassthrough/renderDelegate.h"
#include "pxr/imaging/hd/renderPass.h"

PXR_NAMESPACE_OPEN_SCOPE

/// \class HydraPassthroughRenderPass
///
/// HdRenderPass represents a single render iteration, rendering a view of the
/// scene (the HdRprimCollection) for a specific viewer (the camera/viewport
/// parameters in HdRenderPassState) to the current draw target.
///
/// In this implementation the draw target is remote, so all we do is prepare the
/// data for a remote renderer to consume.
class HydraPassthroughRenderPass final : public HdRenderPass {
public:
    /// Renderpass constructor.
    ///   \param index The render index containing scene data to render.
    ///   \param collection The initial rprim collection for this renderpass.
    ///   \param renderDelegate The render delegate that owns this renderpass.
    HydraPassthroughRenderPass(
            HdRenderIndex *index,
            HdRprimCollection const &collection,
            HydraPassthroughRenderDelegate *renderDelegate);

    /// Renderpass destructor.
    virtual ~HydraPassthroughRenderPass();

protected:
    /// Draw the scene with the bound renderpass state.
    ///   \param renderPassState Input parameters (including viewer parameters)
    ///                          for this renderpass.
    ///   \param renderTags Which rendertags should be drawn this pass.
    void _Execute(HdRenderPassStateSharedPtr const &renderPassState,
                  TfTokenVector const &renderTags) override;

private:
    HydraPassthroughRenderDelegate *_renderDelegate = nullptr;
};

PXR_NAMESPACE_CLOSE_SCOPE

#endif // USD_IMAGING_HYDRA_PASSTHROUGH_RENDER_PASS_H
