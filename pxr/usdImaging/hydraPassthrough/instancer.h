#ifndef USD_IMAGING_HYDRA_PASSTHROUGH_INSTANCER_H
#define USD_IMAGING_HYDRA_PASSTHROUGH_INSTANCER_H

#include "pxr/pxr.h"

#include "pxr/imaging/hd/instancer.h"

#include "pxr/base/tf/declarePtrs.h"
#include "pxr/base/tf/hashmap.h"
#include "pxr/base/tf/token.h"
#include "pxr/base/vt/array.h"
#include "pxr/base/vt/types.h"
#include "pxr/base/vt/value.h"

PXR_NAMESPACE_OPEN_SCOPE

TF_DECLARE_REF_PTRS(HydraPassthroughRenderData);
TF_DECLARE_REF_PTRS(HdSceneIndexBase);

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
/// Sync() caches the instance primvars. ComputeInstanceData() is then
/// called by prototype rprims during their own Sync to compose the cached
/// primvars into per-instance transforms, indices and primvar values,
/// recursing into the parent instancer when instancers are nested.
///
class HydraPassthroughInstancer final : public HdInstancer {
public:
    HydraPassthroughInstancer(HdSceneDelegate *delegate, SdfPath const &id);

    ~HydraPassthroughInstancer() override;

    void Sync(HdSceneDelegate *sceneDelegate,
              HdRenderParam *renderParam,
              HdDirtyBits *dirtyBits) override;

    /// The flattened per-instance data for one prototype. The three members
    /// are parallel: entry k of each describes the same drawn instance.
    struct InstanceData {
        /// One transform per instance, following the composition rules
        /// described in pxr/imaging/hd/instancer.h. Each matrix takes the
        /// prototype from instancer space to world space; it does not
        /// include the prototype rprim's own transform.
        VtMatrix4dArray transforms;

        /// For each instance, the instance index at the instancer level
        /// closest to the prototype. For point instancers these are indices
        /// into the authored point arrays (masked instances are omitted,
        /// not renumbered).
        VtIntArray instanceIndices;

        /// Non-transform instance-interpolation primvars, gathered by
        /// instance index into arrays parallel to transforms. The
        /// transform-building primvars (hydra:instanceTranslations/
        /// Rotations/Scales/Transforms) are excluded since they are baked
        /// into the transforms. When nested instancers author the same
        /// primvar, the level closest to the prototype wins.
        TfHashMap<TfToken, VtValue, TfToken::HashFunctor> primvars;
    };

    /// Compute the flattened instance data for each instance of prototypeId
    /// drawn by this instancer, replacing the contents of instanceData.
    ///
    /// Nested instancing is flattened here by recursing into the parent
    /// instancer. The result is ordered outer-major: all instances for the
    /// first parent instance, then all instances for the second, and so on.
    ///
    /// Produces empty data if the instancer is invisible.
    void ComputeInstanceData(SdfPath const &prototypeId,
                             InstanceData *instanceData);

    /// Collects the origin scene prim of every native (scene graph) instance
    /// from the scene index into the render data, for pick mapping.
    ///
    /// Should be called after the render, when the scene index is fully
    /// populated.
    ///
    /// Output is placed directly into the renderData.
    static void PopulateInstancerOrigins(
        HdSceneIndexBaseRefPtr const &sceneIndex,
        HydraPassthroughRenderDataRefPtr const &renderData,
        SdfPath const &sceneDelegateId);

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
