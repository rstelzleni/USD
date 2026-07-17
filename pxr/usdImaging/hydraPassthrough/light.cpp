#include "pxr/usdImaging/hydraPassthrough/light.h"
#include "pxr/usdImaging/hydraPassthrough/renderData.h"
#include "pxr/usdImaging/hydraPassthrough/renderParam.h"

#include "pxr/imaging/hd/sceneDelegate.h"
#include "pxr/imaging/hd/tokens.h"
#include "pxr/usd/sdf/assetPath.h"
#include "pxr/base/tf/staticTokens.h"

PXR_NAMESPACE_OPEN_SCOPE

// treatAsPoint and treatAsLine have no HdLightTokens entry, but the
// usdImaging light data source resolves bare parameter names against the
// prim's inputs: attributes, schema fallbacks included.
TF_DEFINE_PRIVATE_TOKENS(
    _tokens,
    (treatAsPoint)
    (treatAsLine)
);

namespace {

using LightData = HydraPassthroughRenderData::LightData;

LightData::Type
_LightTypeFromToken(TfToken const& typeId)
{
    if (typeId == HdPrimTypeTokens->cylinderLight) {
        return LightData::Type::Cylinder;
    } else if (typeId == HdPrimTypeTokens->diskLight) {
        return LightData::Type::Disk;
    } else if (typeId == HdPrimTypeTokens->distantLight) {
        return LightData::Type::Distant;
    } else if (typeId == HdPrimTypeTokens->domeLight) {
        return LightData::Type::Dome;
    } else if (typeId == HdPrimTypeTokens->rectLight) {
        return LightData::Type::Rect;
    } else if (typeId == HdPrimTypeTokens->sphereLight) {
        return LightData::Type::Sphere;
    }
    return LightData::Type::Unknown;
}

// Fetch a light parameter, leaving *out untouched (at its schema default)
// if the parameter is missing or holds an unexpected type.
template <typename T>
void
_GetParam(HdSceneDelegate* sceneDelegate,
          SdfPath const& id,
          TfToken const& name,
          T* out)
{
    const VtValue v = sceneDelegate->GetLightParamValue(id, name);
    if (v.IsHolding<T>()) {
        *out = v.UncheckedGet<T>();
    }
}

// Fetch an asset path parameter as a resolved path string, falling back
// to the raw authored path (e.g. for paths that don't resolve locally).
void
_GetAssetPathParam(HdSceneDelegate* sceneDelegate,
                   SdfPath const& id,
                   TfToken const& name,
                   std::string* out)
{
    const VtValue v = sceneDelegate->GetLightParamValue(id, name);
    if (v.IsHolding<SdfAssetPath>()) {
        const SdfAssetPath& p = v.UncheckedGet<SdfAssetPath>();
        *out = p.GetResolvedPath();
        if (out->empty()) {
            *out = p.GetAssetPath();
        }
    } else if (v.IsHolding<std::string>()) {
        *out = v.UncheckedGet<std::string>();
    }
}

} // anonymous namespace

HydraPassthroughLight::HydraPassthroughLight(TfToken const& typeId,
                                             SdfPath const& id)
    : HdLight(id)
    , _lightType(typeId) {
}

HydraPassthroughLight::~HydraPassthroughLight() {
}

HdDirtyBits
HydraPassthroughLight::GetInitialDirtyBitsMask() const
{
    return AllDirty;
}

void
HydraPassthroughLight::Sync(HdSceneDelegate *sceneDelegate,
                            HdRenderParam   *renderParam,
                            HdDirtyBits     *dirtyBits)
{
    auto rp = dynamic_cast<HydraPassthroughRenderParam*>(renderParam);
    if (!rp) {
        TF_CODING_ERROR("HydraPassthroughLight::Sync: "
                  "renderParam is not a HydraPassthroughRenderParam, "
                  "cannot proceed.");
        *dirtyBits = Clean;
        return;
    }

    const SdfPath& id = GetId();

    // Lights carry few parameters and sync rarely, so rather than
    // tracking individual dirty bits we re-read the full light state on
    // any change and republish it.
    LightData light;
    light.id = id;
    light.type = _LightTypeFromToken(_lightType);
    light.transform = sceneDelegate->GetTransform(id);
    // Note that DirtyParams doubles for visibility changes on lights,
    // there is no DirtyVisibility bit.
    light.visible = sceneDelegate->GetVisible(id);

    _GetParam(sceneDelegate, id, HdLightTokens->intensity, &light.intensity);
    _GetParam(sceneDelegate, id, HdLightTokens->exposure, &light.exposure);
    _GetParam(sceneDelegate, id, HdLightTokens->color, &light.color);
    _GetParam(sceneDelegate, id, HdLightTokens->enableColorTemperature,
              &light.enableColorTemperature);
    _GetParam(sceneDelegate, id, HdLightTokens->colorTemperature,
              &light.colorTemperature);
    _GetParam(sceneDelegate, id, HdLightTokens->normalize, &light.normalize);
    _GetParam(sceneDelegate, id, HdLightTokens->diffuse, &light.diffuse);
    _GetParam(sceneDelegate, id, HdLightTokens->specular, &light.specular);

    _GetParam(sceneDelegate, id, HdLightTokens->radius, &light.radius);
    _GetParam(sceneDelegate, id, HdLightTokens->length, &light.length);
    _GetParam(sceneDelegate, id, HdLightTokens->width, &light.width);
    _GetParam(sceneDelegate, id, HdLightTokens->height, &light.height);
    _GetParam(sceneDelegate, id, HdLightTokens->angle, &light.angle);
    _GetParam(sceneDelegate, id, _tokens->treatAsPoint, &light.treatAsPoint);
    _GetParam(sceneDelegate, id, _tokens->treatAsLine, &light.treatAsLine);
    _GetAssetPathParam(sceneDelegate, id, HdLightTokens->textureFile,
                       &light.textureFile);
    _GetParam(sceneDelegate, id, HdLightTokens->textureFormat,
              &light.textureFormat);

    _GetParam(sceneDelegate, id, HdLightTokens->shapingFocus,
              &light.shapingFocus);
    _GetParam(sceneDelegate, id, HdLightTokens->shapingFocusTint,
              &light.shapingFocusTint);
    _GetParam(sceneDelegate, id, HdLightTokens->shapingConeAngle,
              &light.shapingConeAngle);
    _GetParam(sceneDelegate, id, HdLightTokens->shapingConeSoftness,
              &light.shapingConeSoftness);
    _GetAssetPathParam(sceneDelegate, id, HdLightTokens->shapingIesFile,
                       &light.shapingIesFile);
    _GetParam(sceneDelegate, id, HdLightTokens->shapingIesAngleScale,
              &light.shapingIesAngleScale);
    _GetParam(sceneDelegate, id, HdLightTokens->shapingIesNormalize,
              &light.shapingIesNormalize);

    _GetParam(sceneDelegate, id, HdLightTokens->shadowEnable,
              &light.shadowEnable);
    _GetParam(sceneDelegate, id, HdLightTokens->shadowColor,
              &light.shadowColor);
    _GetParam(sceneDelegate, id, HdLightTokens->shadowDistance,
              &light.shadowDistance);
    _GetParam(sceneDelegate, id, HdLightTokens->shadowFalloff,
              &light.shadowFalloff);
    _GetParam(sceneDelegate, id, HdLightTokens->shadowFalloffGamma,
              &light.shadowFalloffGamma);

    rp->GetRenderData()->AddLight(id, light);

    *dirtyBits = Clean;
}

PXR_NAMESPACE_CLOSE_SCOPE
