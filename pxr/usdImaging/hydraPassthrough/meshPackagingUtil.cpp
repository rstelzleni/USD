#include "meshPackagingUtil.h"

#include "pxr/imaging/hd/meshUtil.h"

#include "pxr/base/vt/value.h"
#include "pxr/base/vt/visitValue.h"
#include "pxr/base/vt/typeHeaders.h"

#include <numeric>

PXR_NAMESPACE_OPEN_SCOPE

namespace {

// Gathers array elements: result[i] = array[cornerIndices[i]].
//
// Returns an empty VtValue if the value is not an array or an index is
// out of range.
struct _GatherElementsVisitor {
    const VtIntArray& cornerIndices;

    template <typename T>
    VtValue operator()(const VtArray<T>& array) const {
        const int numElements = (int)array.size();
        VtArray<T> result(cornerIndices.size());
        T* dst = result.data();
        const T* src = array.cdata();
        for (size_t i = 0; i < cornerIndices.size(); ++i) {
            const int index = cornerIndices[i];
            if (index < 0 || index >= numElements) {
                return VtValue();
            }
            dst[i] = src[index];
        }
        return VtValue(result);
    }

    // Fallback for non-array values, which cannot be gathered.
    VtValue operator()(const VtValue& value) const {
        return VtValue();
    }
};

VtValue
_GatherElements(const VtValue& value, const VtIntArray& cornerIndices)
{
    return VtVisitValue(value, _GatherElementsVisitor{cornerIndices});
}

// Flattens a face-varying channel's index buffer (one entry per refined
// patch) into one index per corner of the mesh's faceVertexIndices buffer.
bool
_FlattenFvarIndices(
    const VtValue& indices,
    size_t numCorners,
    VtIntArray* flattened)
{
    if (indices.IsHolding<VtArray<GfVec3i>>()) {
        // Triangle patches (loop scheme) map 1:1 onto the triangle list.
        const VtArray<GfVec3i>& tris = indices.UncheckedGet<VtArray<GfVec3i>>();
        if (tris.size() * 3 != numCorners) {
            return false;
        }
        flattened->resize(numCorners);
        int* dst = flattened->data();
        for (const GfVec3i& tri : tris) {
            *dst++ = tri[0];
            *dst++ = tri[1];
            *dst++ = tri[2];
        }
        return true;
    }
    if (indices.IsHolding<VtArray<GfVec4i>>()) {
        // Quad patches (catmark/bilinear schemes). The mesh's index buffer
        // was emitted by HdMeshTriQuadBuilder, which splits each quad into
        // the triangles (0,1,2) and (2,3,0); apply the same split here so
        // the corners line up.
        const VtArray<GfVec4i>& quads = indices.UncheckedGet<VtArray<GfVec4i>>();
        if (quads.size() * 6 != numCorners) {
            return false;
        }
        flattened->resize(numCorners);
        int* dst = flattened->data();
        for (const GfVec4i& quad : quads) {
            *dst++ = quad[0];
            *dst++ = quad[1];
            *dst++ = quad[2];
            *dst++ = quad[2];
            *dst++ = quad[3];
            *dst++ = quad[0];
        }
        return true;
    }
    // Adaptive refinement (bspline/box spline patches) is not supported.
    return false;
}

// Builds one coarse-face index per corner so uniform (per-face) primvars
// can be expanded. primitiveParam has one entry per triangle for unrefined
// meshes (int) and one entry per patch for refined meshes (GfVec3i with
// the encoded param in element [0]). The number of corners per entry
// (3 for triangles, 6 for triangulated quads) falls out of the buffer
// sizes.
bool
_BuildUniformCornerIndices(
    const VtValue& primitiveParam,
    size_t numCorners,
    VtIntArray* cornerIndices)
{
    VtIntArray encodedParams;
    if (primitiveParam.IsHolding<VtIntArray>()) {
        encodedParams = primitiveParam.UncheckedGet<VtIntArray>();
    } else if (primitiveParam.IsHolding<VtArray<GfVec3i>>()) {
        const VtArray<GfVec3i>& params =
            primitiveParam.UncheckedGet<VtArray<GfVec3i>>();
        encodedParams.resize(params.size());
        for (size_t i = 0; i < params.size(); ++i) {
            encodedParams[i] = params[i][0];
        }
    } else {
        return false;
    }

    if (encodedParams.empty() || numCorners % encodedParams.size() != 0) {
        return false;
    }
    const size_t cornersPerEntry = numCorners / encodedParams.size();
    if (cornersPerEntry != 3 && cornersPerEntry != 6) {
        return false;
    }

    cornerIndices->resize(numCorners);
    int* dst = cornerIndices->data();
    for (const int encodedParam : encodedParams) {
        const int faceIndex =
            HdMeshUtil::DecodeFaceIndexFromCoarseFaceParam(encodedParam);
        for (size_t corner = 0; corner < cornersPerEntry; ++corner) {
            *dst++ = faceIndex;
        }
    }
    return true;
}

} // anonymous namespace

namespace HydraPassthroughMeshPackagingUtil
{

void
DeindexMesh(HydraPassthroughRenderData::MeshData* meshData)
{
    // Keep a copy of the original vertex indices; they are overwritten at
    // the end.
    const VtIntArray vertexIndices = meshData->faceVertexIndices;
    const size_t numCorners = vertexIndices.size();
    if (numCorners == 0) {
        return;
    }

    // Refined face-varying primvars hold compact value buffers that their
    // channel's index buffer maps onto corners; expand them through those
    // indices. Unrefined face-varying primvars have no channel and are
    // already per-corner.
    for (const HydraPassthroughRenderData::FaceVaryingChannel& channel :
            meshData->faceVaryingChannels) {
        VtIntArray cornerIndices;
        if (!_FlattenFvarIndices(channel.indices, numCorners,
                                 &cornerIndices)) {
            TF_WARN("Face-varying channel %d of %s has indices that don't "
                    "match the mesh's triangulated topology; leaving its "
                    "primvars unexpanded",
                    channel.channel, meshData->id.GetText());
            continue;
        }
        for (const TfToken& name : channel.primvars) {
            auto primvarIt = meshData->primvars.find(name);
            if (primvarIt == meshData->primvars.end()) {
                continue;
            }
            const VtValue expanded =
                _GatherElements(primvarIt->second.data, cornerIndices);
            if (expanded.IsEmpty()) {
                TF_WARN("Could not expand face-varying primvar %s of %s",
                        name.GetText(), meshData->id.GetText());
                continue;
            }
            primvarIt->second.data = expanded;
        }
    }
    meshData->faceVaryingChannels.clear();

    // Built on demand if we encounter a uniform primvar.
    VtIntArray uniformCornerIndices;
    bool builtUniformCornerIndices = false;

    for (auto& primvarEntry : meshData->primvars) {
        const TfToken& name = primvarEntry.first;
        HydraPassthroughRenderData::PrimvarData& primvar =
            primvarEntry.second;

        switch (primvar.interpolation) {
            case HdInterpolationVertex:
            case HdInterpolationVarying: {
                const VtValue expanded =
                    _GatherElements(primvar.data, vertexIndices);
                if (expanded.IsEmpty()) {
                    TF_WARN("Could not expand vertex/varying primvar %s of %s",
                            name.GetText(), meshData->id.GetText());
                    break;
                }
                primvar.data = expanded;
                primvar.interpolation = HdInterpolationFaceVarying;
                break;
            }
            case HdInterpolationUniform: {
                if (!builtUniformCornerIndices) {
                    builtUniformCornerIndices = true;
                    if (!_BuildUniformCornerIndices(
                            meshData->primitiveParam, numCorners,
                            &uniformCornerIndices)) {
                        TF_WARN("Could not map faces to corners for %s; "
                                "leaving uniform primvars unexpanded",
                                meshData->id.GetText());
                    }
                }
                if (uniformCornerIndices.empty()) {
                    break;
                }
                const VtValue expanded =
                    _GatherElements(primvar.data, uniformCornerIndices);
                if (expanded.IsEmpty()) {
                    TF_WARN("Could not expand uniform primvar %s of %s",
                            name.GetText(), meshData->id.GetText());
                    break;
                }
                primvar.data = expanded;
                primvar.interpolation = HdInterpolationFaceVarying;
                break;
            }
            case HdInterpolationFaceVarying: {
                // Already one value per corner; just sanity check.
                if (primvar.data.GetArraySize() != numCorners) {
                    TF_WARN("Face-varying primvar %s of %s has %zu values, "
                            "expected %zu",
                            name.GetText(), meshData->id.GetText(),
                            primvar.data.GetArraySize(), numCorners);
                }
                break;
            }
            default:
                // Constant and other interpolations are unaffected.
                break;
        }
    }

    // The points member mirrors the points primvar; keep them consistent.
    if (!meshData->points.IsEmpty()) {
        const VtValue expanded =
            _GatherElements(meshData->points, vertexIndices);
        if (!expanded.IsEmpty()) {
            meshData->points = expanded;
        }
    }

    // Every buffer is per-corner now, so the indices become the identity.
    VtIntArray identity(numCorners);
    std::iota(identity.begin(), identity.end(), 0);
    meshData->faceVertexIndices = identity;
}

} // namespace HydraPassthroughMeshPackagingUtil

PXR_NAMESPACE_CLOSE_SCOPE
