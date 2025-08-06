//
// Copyright 2020 Pixar
//
// Licensed under the terms set forth in the LICENSE.txt file available at
// https://openusd.org/license.
//
#include "renderPass.h"

#include <iostream>

PXR_NAMESPACE_OPEN_SCOPE

HdTinyRenderPass::HdTinyRenderPass(HdRenderIndex *index,
                                   HdRprimCollection const &collection)
    : HdRenderPass(index, collection) {}

HdTinyRenderPass::~HdTinyRenderPass() {
    std::cout << "Destroying renderPass" << std::endl;
}

void HdTinyRenderPass::_Execute(
    HdRenderPassStateSharedPtr const &renderPassState,
    TfTokenVector const &renderTags) {
    std::cout << "=> Execute RenderPass" << std::endl;


    const auto &drawableRprims =
        GetRenderIndex()->GetDrawItems(GetRprimCollection(), renderTags);

    for (const auto &rPrim : drawableRprims) {
        std::cout << "Rendering RPrim: " << rPrim->GetRprimID() << std::endl;
        // Here you would typically call the render delegate to draw the rPrim.
        // For this example, we just print the path.
    }
}

PXR_NAMESPACE_CLOSE_SCOPE
