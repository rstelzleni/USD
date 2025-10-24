#ifndef USD_IMAGING_HYDRA_PASSTHROUGH_RENDER_PARAM_H
#define USD_IMAGING_HYDRA_PASSTHROUGH_RENDER_PARAM_H


#include "pxr/pxr.h"
#include "pxr/usdImaging/hydraPassthrough/renderData.h"
#include "pxr/imaging/hd/renderDelegate.h" // for HdRenderParam

PXR_NAMESPACE_OPEN_SCOPE

using HioGlslfxSharedPtr = std::shared_ptr<class HioGlslfx>;
using HydraPassthroughGlslfxCache = std::unordered_map<size_t, HioGlslfxSharedPtr>;

class HydraPassthroughRenderParam : public HdRenderParam {
public:
    HydraPassthroughRenderParam(HydraPassthroughRenderDataRefPtr renderData)
        : _renderData(renderData) {}
    ~HydraPassthroughRenderParam() override = default;

    // Get the render data associated with this render param.
    const HydraPassthroughRenderDataRefPtr &GetRenderData() const {
        return _renderData;
    }

    // Get the glslfx cache associated with this render param.
    HydraPassthroughGlslfxCache &GetGlslfxCache() {
        return _glslfxCache;
    }

private:
    HydraPassthroughRenderDataRefPtr _renderData;
    HydraPassthroughGlslfxCache _glslfxCache;
};

PXR_NAMESPACE_CLOSE_SCOPE

#endif // USD_IMAGING_HYDRA_PASSTHROUGH_RENDER_PARAM_H
