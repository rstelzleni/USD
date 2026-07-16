#ifndef USD_IMAGING_HYDRA_PASSTHROUGH_LIGHT_H
#define USD_IMAGING_HYDRA_PASSTHROUGH_LIGHT_H

#include "pxr/pxr.h"

#include "pxr/imaging/hd/light.h"

PXR_NAMESPACE_OPEN_SCOPE

/// \class HydraPassthroughLight
///
/// Light sprim that captures UsdLux light state at sync time and publishes
/// it into the render data output as a
/// HydraPassthroughRenderData::LightData.
///
class HydraPassthroughLight final : public HdLight {
public:
    HydraPassthroughLight(TfToken const& typeId, SdfPath const& id);
    ~HydraPassthroughLight() override;

    /// Synchronizes state from the delegate to this object.
    void Sync(HdSceneDelegate *sceneDelegate,
              HdRenderParam   *renderParam,
              HdDirtyBits     *dirtyBits) override;

    /// Tell the change tracker we need to sync everything.
    HdDirtyBits GetInitialDirtyBitsMask() const override;

private:
    // The sprim type this light was created as (e.g. sphereLight),
    // which determines the LightData type reported to clients.
    TfToken _lightType;
};

PXR_NAMESPACE_CLOSE_SCOPE

#endif // USD_IMAGING_HYDRA_PASSTHROUGH_LIGHT_H
