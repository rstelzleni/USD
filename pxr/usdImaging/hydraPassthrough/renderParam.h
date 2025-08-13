#pragma once

#include "pxr/pxr.h"
#include "pxr/usdImaging/hydraPassthrough/renderData.h"
#include "pxr/imaging/hd/renderDelegate.h" // for HdRenderParam

PXR_NAMESPACE_OPEN_SCOPE

class HydraPassthroughRenderParam : public HdRenderParam {
public:
    HydraPassthroughRenderParam(HydraPassthroughRenderDataRefPtr renderData)
        : _renderData(renderData) {}
    ~HydraPassthroughRenderParam() override = default;

    // Get the render data associated with this render param.
    const HydraPassthroughRenderDataRefPtr &GetRenderData() const {
        return _renderData;
    }

private:
    HydraPassthroughRenderDataRefPtr _renderData;
};

PXR_NAMESPACE_CLOSE_SCOPE
