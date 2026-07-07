#include "instancer.h"

#include "pxr/imaging/hd/changeTracker.h"
#include "pxr/imaging/hd/renderIndex.h"
#include "pxr/imaging/hd/sceneDelegate.h"
#include "pxr/imaging/hd/tokens.h"

#include "pxr/base/gf/matrix4d.h"
#include "pxr/base/gf/matrix4f.h"
#include "pxr/base/gf/quatd.h"
#include "pxr/base/gf/quatf.h"
#include "pxr/base/gf/quath.h"
#include "pxr/base/gf/vec3d.h"
#include "pxr/base/gf/vec3f.h"
#include "pxr/base/gf/vec4d.h"
#include "pxr/base/gf/vec4f.h"
#include "pxr/base/tf/stl.h"

PXR_NAMESPACE_OPEN_SCOPE

namespace {

// The scene index emulation hands us the instance primvars as VtValues
// holding arrays. Which held type we see depends on the source: point
// instancers author vec3f positions/scales and quath (or quatf)
// orientations, while the native instance aggregation scene index produces
// matrix4d transforms. These helpers sample one element as double
// precision, returning false if the value holds an unexpected type or the
// index is out of range. Callers treat a false return as identity.

bool
_SampleVec3(const VtValue &value, int index, GfVec3d *out)
{
    const size_t i = static_cast<size_t>(index);
    if (value.IsHolding<VtVec3fArray>()) {
        const VtVec3fArray &array = value.UncheckedGet<VtVec3fArray>();
        if (i < array.size()) {
            *out = GfVec3d(array[i]);
            return true;
        }
    } else if (value.IsHolding<VtVec3dArray>()) {
        const VtVec3dArray &array = value.UncheckedGet<VtVec3dArray>();
        if (i < array.size()) {
            *out = array[i];
            return true;
        }
    }
    return false;
}

bool
_SampleQuat(const VtValue &value, int index, GfQuatd *out)
{
    const size_t i = static_cast<size_t>(index);
    if (value.IsHolding<VtQuathArray>()) {
        const VtQuathArray &array = value.UncheckedGet<VtQuathArray>();
        if (i < array.size()) {
            *out = GfQuatd(array[i]);
            return true;
        }
    } else if (value.IsHolding<VtQuatfArray>()) {
        const VtQuatfArray &array = value.UncheckedGet<VtQuatfArray>();
        if (i < array.size()) {
            *out = GfQuatd(array[i]);
            return true;
        }
    } else if (value.IsHolding<VtQuatdArray>()) {
        const VtQuatdArray &array = value.UncheckedGet<VtQuatdArray>();
        if (i < array.size()) {
            *out = array[i];
            return true;
        }
    } else if (value.IsHolding<VtVec4fArray>()) {
        // Quaternion packed as <real, i, j, k>
        const VtVec4fArray &array = value.UncheckedGet<VtVec4fArray>();
        if (i < array.size()) {
            const GfVec4f &q = array[i];
            *out = GfQuatd(q[0], q[1], q[2], q[3]);
            return true;
        }
    } else if (value.IsHolding<VtVec4dArray>()) {
        const VtVec4dArray &array = value.UncheckedGet<VtVec4dArray>();
        if (i < array.size()) {
            const GfVec4d &q = array[i];
            *out = GfQuatd(q[0], q[1], q[2], q[3]);
            return true;
        }
    }
    return false;
}

bool
_SampleMatrix(const VtValue &value, int index, GfMatrix4d *out)
{
    const size_t i = static_cast<size_t>(index);
    if (value.IsHolding<VtMatrix4dArray>()) {
        const VtMatrix4dArray &array = value.UncheckedGet<VtMatrix4dArray>();
        if (i < array.size()) {
            *out = array[i];
            return true;
        }
    } else if (value.IsHolding<VtMatrix4fArray>()) {
        const VtMatrix4fArray &array = value.UncheckedGet<VtMatrix4fArray>();
        if (i < array.size()) {
            *out = GfMatrix4d(array[i]);
            return true;
        }
    }
    return false;
}

} // anonymous namespace

HydraPassthroughInstancer::HydraPassthroughInstancer(
    HdSceneDelegate *delegate, SdfPath const &id)
    : HdInstancer(delegate, id)
{
}

HydraPassthroughInstancer::~HydraPassthroughInstancer() = default;

void
HydraPassthroughInstancer::Sync(HdSceneDelegate *sceneDelegate,
                                HdRenderParam *renderParam,
                                HdDirtyBits *dirtyBits)
{
    if (*dirtyBits & HdChangeTracker::DirtyVisibility) {
        _visible = sceneDelegate->GetVisible(GetId());
    }

    _UpdateInstancer(sceneDelegate, dirtyBits);

    if (HdChangeTracker::IsAnyPrimvarDirty(*dirtyBits, GetId())) {
        _SyncPrimvars(sceneDelegate, *dirtyBits);
    }
}

void
HydraPassthroughInstancer::_SyncPrimvars(HdSceneDelegate *sceneDelegate,
                                         HdDirtyBits dirtyBits)
{
    HD_TRACE_FUNCTION();
    HF_MALLOC_TAG_FUNCTION();

    SdfPath const &id = GetId();

    HdPrimvarDescriptorVector primvars =
        sceneDelegate->GetPrimvarDescriptors(id, HdInterpolationInstance);

    // Drop cached values for primvars that no longer exist. A removal
    // arrives as a primvar-dirty notification, and the removed name simply
    // stops being listed in the descriptors.
    TfTokenVector removedNames;
    for (const auto &entry : _primvarMap) {
        bool stillExists = false;
        for (const HdPrimvarDescriptor &pv : primvars) {
            if (pv.name == entry.first) {
                stillExists = true;
                break;
            }
        }
        if (!stillExists) {
            removedNames.push_back(entry.first);
        }
    }
    for (const TfToken &name : removedNames) {
        _primvarMap.erase(name);
    }

    for (const HdPrimvarDescriptor &pv : primvars) {
        if (HdChangeTracker::IsPrimvarDirty(dirtyBits, id, pv.name)) {
            VtValue value = sceneDelegate->Get(id, pv.name);
            if (value.IsEmpty()) {
                _primvarMap.erase(pv.name);
            } else {
                _primvarMap[pv.name] = value;
            }
        }
    }
}

VtMatrix4dArray
HydraPassthroughInstancer::ComputeInstanceTransforms(
    SdfPath const &prototypeId)
{
    HD_TRACE_FUNCTION();
    HF_MALLOC_TAG_FUNCTION();

    // The transforms for this level of instancer are computed by:
    // foreach(index : indices) {
    //     instancerTransform
    //     * hydra:instanceTranslations(index)
    //     * hydra:instanceRotations(index)
    //     * hydra:instanceScales(index)
    //     * hydra:instanceTransforms(index)
    // }
    // If any transform isn't provided, it's assumed to be the identity.

    if (!_visible) {
        return VtMatrix4dArray();
    }

    const GfMatrix4d instancerTransform =
        GetDelegate()->GetInstancerTransform(GetId());
    const VtIntArray instanceIndices =
        GetDelegate()->GetInstanceIndices(GetId(), prototypeId);

    const VtValue *translations =
        TfMapLookupPtr(_primvarMap, HdInstancerTokens->instanceTranslations);
    const VtValue *rotations =
        TfMapLookupPtr(_primvarMap, HdInstancerTokens->instanceRotations);
    const VtValue *scales =
        TfMapLookupPtr(_primvarMap, HdInstancerTokens->instanceScales);
    const VtValue *instanceTransforms =
        TfMapLookupPtr(_primvarMap, HdInstancerTokens->instanceTransforms);

    VtMatrix4dArray transforms(instanceIndices.size());
    for (size_t i = 0; i < instanceIndices.size(); ++i) {
        const int index = instanceIndices[i];
        GfMatrix4d transform = instancerTransform;

        GfVec3d translate;
        if (translations && _SampleVec3(*translations, index, &translate)) {
            GfMatrix4d translateMat(1);
            translateMat.SetTranslate(translate);
            transform = translateMat * transform;
        }

        GfQuatd rotate;
        if (rotations && _SampleQuat(*rotations, index, &rotate)) {
            GfMatrix4d rotateMat(1);
            rotateMat.SetRotate(rotate);
            transform = rotateMat * transform;
        }

        GfVec3d scale;
        if (scales && _SampleVec3(*scales, index, &scale)) {
            GfMatrix4d scaleMat(1);
            scaleMat.SetScale(scale);
            transform = scaleMat * transform;
        }

        GfMatrix4d instanceTransform;
        if (instanceTransforms &&
            _SampleMatrix(*instanceTransforms, index, &instanceTransform)) {
            transform = instanceTransform * transform;
        }

        transforms[i] = transform;
    }

    if (GetParentId().IsEmpty()) {
        return transforms;
    }

    // Nested instancing: this whole instancer is itself instanced by its
    // parent, so multiply every local transform by every parent transform,
    // flattening outer-major.
    HdInstancer *parentInstancer =
        GetDelegate()->GetRenderIndex().GetInstancer(GetParentId());
    if (!TF_VERIFY(parentInstancer)) {
        return transforms;
    }

    const VtMatrix4dArray parentTransforms =
        static_cast<HydraPassthroughInstancer*>(parentInstancer)->
            ComputeInstanceTransforms(GetId());

    VtMatrix4dArray final(parentTransforms.size() * transforms.size());
    for (size_t i = 0; i < parentTransforms.size(); ++i) {
        for (size_t j = 0; j < transforms.size(); ++j) {
            final[i * transforms.size() + j] =
                transforms[j] * parentTransforms[i];
        }
    }
    return final;
}

PXR_NAMESPACE_CLOSE_SCOPE
