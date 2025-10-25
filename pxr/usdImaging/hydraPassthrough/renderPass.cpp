#include "renderPass.h"


PXR_NAMESPACE_OPEN_SCOPE

HydraPassthroughRenderPass::HydraPassthroughRenderPass(HdRenderIndex *index,
                                   HdRprimCollection const &collection,
                                   HydraPassthroughRenderDelegate *renderDelegate)
    : HdRenderPass(index, collection),
      _renderDelegate(renderDelegate)
{}

HydraPassthroughRenderPass::~HydraPassthroughRenderPass() {
}

void HydraPassthroughRenderPass::_Execute(
    HdRenderPassStateSharedPtr const &renderPassState,
    TfTokenVector const &renderTags) {

    const auto &drawableRprims =
        GetRenderIndex()->GetDrawItems(GetRprimCollection(), renderTags);

    //for (const auto &rPrim : drawableRprims) {
        //std::cout << "Rendering RPrim: " << rPrim->GetRprimID() << std::endl;
        //
        // Here you would typically call the render delegate to draw the rPrim.
        //
        // In reality this is _not_ an rPrim, it's a draw item. Draw items do have
        // information about materials, visibility, etc, and we do need that in
        // addition to the rprim mesh data. We can delegate this to the render
        // delegate, which I think is what other render passes do.
    //}
}

PXR_NAMESPACE_CLOSE_SCOPE
