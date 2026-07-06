#ifndef USD_IMAGING_HYDRA_PASSTHROUGH_INSTANCER_H
#define USD_IMAGING_HYDRA_PASSTHROUGH_INSTANCER_H

#include "pxr/pxr.h"

#include "pxr/imaging/hd/instancer.h"

#include "pxr/base/tf/hashmap.h"
#include "pxr/base/tf/token.h"
#include "pxr/base/vt/types.h"
#include "pxr/base/vt/value.h"

PXR_NAMESPACE_OPEN_SCOPE

/// \class HydraPassthroughInstancer
///
/// Computes flattened per-instance transforms for prototype rprims.
///
/// Both USD instancing styles arrive here in the same form: the usdImaging
/// scene indices convert point instancers and native (instanceable) prims
/// into hydra instancer prims carrying instance-interpolation primvars
/// (hydra:instanceTranslations/Rotations/Scales for point instancers,
/// hydra:instanceTransforms for aggregated native instances), with the
/// prototype meshes pointing back at the instancer via their instancerId.
///
/// Sync() caches the instance primvars. ComputeInstanceTransforms() is then
/// called by prototype rprims during their own Sync to compose the cached
/// primvars into one matrix per instance, recursing into the parent
/// instancer when instancers are nested.
///
class HydraPassthroughInstancer final : public HdInstancer {
public:
    HydraPassthroughInstancer(HdSceneDelegate *delegate, SdfPath const &id);

    ~HydraPassthroughInstancer() override;

    void Sync(HdSceneDelegate *sceneDelegate,
              HdRenderParam *renderParam,
              HdDirtyBits *dirtyBits) override;

    /// Compute the transform for each instance of prototypeId drawn by this
    /// instancer, following the composition rules described in
    /// pxr/imaging/hd/instancer.h. Each returned matrix takes the prototype
    /// from instancer space to world space; it does not include the
    /// prototype rprim's own transform.
    ///
    /// Nested instancing is flattened here by recursing into the parent
    /// instancer. The result is ordered outer-major: all instances for the
    /// first parent instance, then all instances for the second, and so on.
    ///
    /// Returns an empty array if the instancer is invisible.
    VtMatrix4dArray ComputeInstanceTransforms(SdfPath const &prototypeId);

private:
    void _SyncPrimvars(HdSceneDelegate *sceneDelegate, HdDirtyBits dirtyBits);

    bool _visible{true};

    // Cached instance-interpolation primvar values, keyed by primvar name.
    // Written by Sync, which hydra serializes per instancer, and read
    // concurrently afterwards by the prototype rprims' parallel Sync calls.
    TfHashMap<TfToken, VtValue, TfToken::HashFunctor> _primvarMap;

    // This class does not support copying.
    HydraPassthroughInstancer(const HydraPassthroughInstancer &) = delete;
    HydraPassthroughInstancer &operator=(
        const HydraPassthroughInstancer &) = delete;
};

PXR_NAMESPACE_CLOSE_SCOPE

#endif // USD_IMAGING_HYDRA_PASSTHROUGH_INSTANCER_H
