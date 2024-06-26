//
// Copyright 2019 Pixar
//
// Licensed under the Apache License, Version 2.0 (the "Apache License")
// with the following modification; you may not use this file except in
// compliance with the Apache License and the following modification to it:
// Section 6. Trademarks. is deleted and replaced with:
//
// 6. Trademarks. This License does not grant permission to use the trade
//    names, trademarks, service marks, or product names of the Licensor
//    and its affiliates, except as required to comply with Section 4(c) of
//    the License and to reproduce the content of the NOTICE file.
//
// You may obtain a copy of the Apache License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the Apache License with the above modification is
// distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
// KIND, either express or implied. See the Apache License for the specific
// language governing permissions and limitations under the Apache License.
//
#include "hdPrman/camera.h"
#include "hdPrman/cameraContext.h"

#include "pxr/imaging/hd/sceneDelegate.h"

PXR_NAMESPACE_OPEN_SCOPE

#if HD_API_VERSION < 52
TF_DEFINE_PRIVATE_TOKENS(
    _lensDistortionTokens,
    ((k1,     "lensDistortion:k1"))
    ((k2,     "lensDistortion:k2"))
    ((center, "lensDistortion:center"))
    ((anaSq,  "lensDistortion:anaSq"))
    ((asym,   "lensDistortion:asym"))
    ((scale,  "lensDistortion:scale"))
);
#endif

TF_DEFINE_PRIVATE_TOKENS(
    _tokens,
    ((shutterOpenTime,   "ri:shutterOpenTime"))
    ((shutterCloseTime,  "ri:shutterCloseTime"))
    ((shutteropening,    "ri:shutteropening"))
    ((apertureAngle,     "ri:apertureAngle"))
    ((apertureDensity,   "ri:apertureDensity"))
    ((apertureNSides,    "ri:apertureNSides"))
    ((apertureRoundness, "ri:apertureRoundness"))
);

namespace {

const HdPrmanCamera::ShutterCurve&
_GetFallbackShutterCurve(
    bool interactive)
{
    if (interactive) {
        // Open instantaneously, remain fully open for the duration of the
        // shutter interval (set via the param RixStr.k_Ri_Shutter) and close
        // instantaneously.
        static const HdPrmanCamera::ShutterCurve interactiveFallback = {
            0.0,
            1.0,
            { 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f }};

        return interactiveFallback;
    }

    // Open instantaneously and start closing immediately, rapidly at first
    // decelerating until the end of the interval.
    static const HdPrmanCamera::ShutterCurve batchFallback = {
        0.0,
        0.0,
        { 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.3f, 0.0f }};
    
    return batchFallback;
}

} // anon


HdPrmanCamera::HdPrmanCamera(SdfPath const& id)
  : HdCamera(id)
#if HD_API_VERSION < 52
  , _lensDistortionK1(0.0f)
  , _lensDistortionK2(0.0f)
  , _lensDistortionCenter(0.0f)
  , _lensDistortionAnaSq(1.0f)
  , _lensDistortionAsym(0.0f)
  , _lensDistortionScale(1.0f)
#endif
  , _shutterCurve(_GetFallbackShutterCurve(/*isInteractive = */true))
  , _apertureAngle(0.0f)
  , _apertureDensity(0.0f)
  , _apertureNSides(0)
  , _apertureRoundness(1.0f)
{
}

HdPrmanCamera::~HdPrmanCamera() = default;

/* virtual */
void
HdPrmanCamera::Sync(HdSceneDelegate *sceneDelegate,
                    HdRenderParam   *renderParam,
                    HdDirtyBits     *dirtyBits)
{  
    HD_TRACE_FUNCTION();
    HF_MALLOC_TAG_FUNCTION();

    if (!TF_VERIFY(sceneDelegate)) {
        return;
    }

    HdPrman_RenderParam * const param =
        static_cast<HdPrman_RenderParam*>(renderParam);

    SdfPath const &id = GetId();
    // Save state of dirtyBits before HdCamera::Sync clears them.
    const HdDirtyBits bits = *dirtyBits;

    if (bits & DirtyTransform) {
        sceneDelegate->SampleTransform(id, &_sampleXforms);
    }

    if (bits & AllDirty) {
        param->GetCameraContext().MarkCameraInvalid(id);
    }

    HdCamera::Sync(sceneDelegate, renderParam, dirtyBits);

    if (bits & DirtyParams) {
#if HD_API_VERSION < 52
        _lensDistortionK1 =
            sceneDelegate
                ->GetCameraParamValue(id, _lensDistortionTokens->k1)
                .GetWithDefault<float>(0.0f);
        _lensDistortionK2 =
            sceneDelegate
                ->GetCameraParamValue(id, _lensDistortionTokens->k2)
                .GetWithDefault<float>(0.0f);
        _lensDistortionCenter =
            sceneDelegate
                ->GetCameraParamValue(id, _lensDistortionTokens->center)
                .GetWithDefault<GfVec2f>(GfVec2f(0.0f));
        _lensDistortionAnaSq =
            sceneDelegate
                ->GetCameraParamValue(id, _lensDistortionTokens->anaSq)
                .GetWithDefault<float>(1.0f);
        _lensDistortionAsym =
            sceneDelegate
                ->GetCameraParamValue(id, _lensDistortionTokens->asym)
                .GetWithDefault<GfVec2f>(GfVec2f(0.0f));
        _lensDistortionScale =
            sceneDelegate
                ->GetCameraParamValue(id, _lensDistortionTokens->scale)
                .GetWithDefault<float>(1.0f);
#endif

        const VtValue vShutterOpenTime =
            sceneDelegate->GetCameraParamValue(id, _tokens->shutterOpenTime);
        const VtValue vShutterCloseTime =
            sceneDelegate->GetCameraParamValue(id, _tokens->shutterCloseTime);
        const VtValue vShutteropening =
            sceneDelegate->GetCameraParamValue(id, _tokens->shutteropening);
        
        if (vShutterOpenTime.IsHolding<float>() &&
            vShutterCloseTime.IsHolding<float>() &&
            vShutteropening.IsHolding<VtArray<float>>()) {

            _shutterCurve = {
                vShutterOpenTime.UncheckedGet<float>(),
                vShutterCloseTime.UncheckedGet<float>(),
                vShutteropening.UncheckedGet<VtArray<float>>()
            };

        } else {
            _shutterCurve = _GetFallbackShutterCurve(param->IsInteractive());
        }

        _apertureAngle =
            sceneDelegate->GetCameraParamValue(id, _tokens->apertureAngle)
                         .GetWithDefault<float>(0.0f);
        _apertureDensity =
            sceneDelegate->GetCameraParamValue(id, _tokens->apertureDensity)
                         .GetWithDefault<float>(0.0f);
        _apertureNSides =
            sceneDelegate->GetCameraParamValue(id, _tokens->apertureNSides)
                         .GetWithDefault<int>(0);
        _apertureRoundness =
            sceneDelegate->GetCameraParamValue(id, _tokens->apertureRoundness)
                         .GetWithDefault<float>(1.0f);

        if (id == param->GetCameraContext().GetCameraPath()) {
            // Motion blur in Riley only works correctly if the
            // shutter interval is set before any rprims are synced
            // (and the transform of the riley camera is updated).
            //
            // See SetRileyShutterIntervalFromCameraContextCameraPath
            // for additional context.
            //
            param->SetRileyShutterIntervalFromCameraContextCameraPath(
                &sceneDelegate->GetRenderIndex());
        }
    }

    // XXX: Should we flip the proj matrix (RHS vs LHS) as well here?

    // We don't need to clear the dirty bits since HdCamera::Sync always clears
    // all the dirty bits.
}

PXR_NAMESPACE_CLOSE_SCOPE

