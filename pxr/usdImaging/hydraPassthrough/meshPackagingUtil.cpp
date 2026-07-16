#include "meshPackagingUtil.h"

#include "pxr/imaging/hd/meshUtil.h"

#include "pxr/base/vt/value.h"
#include "pxr/base/vt/visitValue.h"
#include "pxr/base/vt/typeHeaders.h"

#include <algorithm>
#include <numeric>
#include <string>
#include <type_traits>
#include <unordered_map>
#include <vector>

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

// Decodes primitiveParam into one authored (coarse) face index per fine
// primitive. primitiveParam has one entry per triangle for unrefined
// meshes (int) and one entry per patch for refined meshes (GfVec3i with
// the encoded param in element [0]).
bool
_DecodePrimitiveFaceIndices(
    const VtValue& primitiveParam,
    VtIntArray* faceIndices)
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

    faceIndices->resize(encodedParams.size());
    int* dst = faceIndices->data();
    for (const int encodedParam : encodedParams) {
        *dst++ = HdMeshUtil::DecodeFaceIndexFromCoarseFaceParam(encodedParam);
    }
    return true;
}

// Builds one coarse-face index per corner so uniform (per-face) primvars
// can be expanded. The number of corners per primitive (3 for triangles,
// 6 for triangulated quads) falls out of the buffer sizes.
bool
_BuildUniformCornerIndices(
    const VtValue& primitiveParam,
    size_t numCorners,
    VtIntArray* cornerIndices)
{
    VtIntArray faceIndices;
    if (!_DecodePrimitiveFaceIndices(primitiveParam, &faceIndices)) {
        return false;
    }

    if (faceIndices.empty() || numCorners % faceIndices.size() != 0) {
        return false;
    }
    const size_t cornersPerEntry = numCorners / faceIndices.size();
    if (cornersPerEntry != 3 && cornersPerEntry != 6) {
        return false;
    }

    cornerIndices->resize(numCorners);
    int* dst = cornerIndices->data();
    for (const int faceIndex : faceIndices) {
        for (size_t corner = 0; corner < cornersPerEntry; ++corner) {
            *dst++ = faceIndex;
        }
    }
    return true;
}

// Builds a per-element "first occurrence" index column for an array value:
// elements that are bitwise equal map to the index of their first
// occurrence. Welding uses this to detect where per-corner values agree
// exactly.
//
// Bitwise comparison means the result is always correct: it can only ever
// split more vertices than strictly needed, never weld values that differ.
// Element types that are not trivially copyable fall back to the identity
// column (every corner unique), and non-array values produce an empty
// column.
struct _BuildFirstOccurrenceColumnVisitor {
    template <typename T>
    VtIntArray operator()(const VtArray<T>& array) const {
        VtIntArray column(array.size());
        if constexpr (std::is_trivially_copyable_v<T>) {
            // Note: element types with internal padding could produce
            // false splits here, but the arithmetic and Gf types used for
            // primvars have none.
            const T* src = array.cdata();
            std::unordered_map<std::string, int> firstOccurrence;
            firstOccurrence.reserve(array.size());
            int* dst = column.data();
            for (size_t i = 0; i < array.size(); ++i) {
                const std::string bytes(
                    reinterpret_cast<const char*>(src + i), sizeof(T));
                const auto insertion = firstOccurrence.emplace(bytes, (int)i);
                dst[i] = insertion.first->second;
            }
        } else {
            std::iota(column.begin(), column.end(), 0);
        }
        return column;
    }

    // Fallback for non-array values, which have no elements to weld.
    VtIntArray operator()(const VtValue& value) const {
        return VtIntArray();
    }
};

// Hash for a weld key (one index per column).
struct _CornerKeyHash {
    size_t operator()(const std::vector<int>& key) const {
        size_t hash = 0;
        for (const int value : key) {
            hash ^= std::hash<int>()(value) +
                    0x9e3779b9 + (hash << 6) + (hash >> 2);
        }
        return hash;
    }
};

} // anonymous namespace

namespace HydraPassthroughMeshPackagingUtil
{

void
BuildDrawGroups(HydraPassthroughRenderData::MeshData* meshData)
{
    meshData->drawGroups.clear();
    if (meshData->geomSubsets.empty()) {
        return;
    }

    // Every fine primitive maps back to the authored face it came from
    // through primitiveParam, and geom subsets are lists of authored
    // faces, so decoding primitiveParam assigns each primitive to its
    // subset. Holes are handled implicitly: no primitive decodes to a
    // hole face.
    const size_t numCorners = meshData->faceVertexIndices.size();
    VtIntArray primFaceIndices;
    if (numCorners == 0 ||
        !_DecodePrimitiveFaceIndices(meshData->primitiveParam,
                                     &primFaceIndices) ||
        primFaceIndices.empty() ||
        numCorners % primFaceIndices.size() != 0) {
        TF_WARN("Could not map primitives to faces for %s; geom subsets "
                "will not be exported as draw groups",
                meshData->id.GetText());
        return;
    }
    const size_t numPrims = primFaceIndices.size();
    const size_t cornersPerPrim = numCorners / numPrims;

    // Assign each authored face to the subset that lists it (sanitizing
    // already removed duplicates between subsets). Faces in no subset form
    // the trailing remainder group, drawn with the mesh's own material.
    const size_t numSubsets = meshData->geomSubsets.size();
    const size_t remainderGroup = numSubsets;
    const size_t numGroups = numSubsets + 1;
    std::unordered_map<int, size_t> faceToGroup;
    for (size_t subset = 0; subset < numSubsets; ++subset) {
        for (const int face : meshData->geomSubsets[subset].faceIndices) {
            faceToGroup.emplace(face, subset);
        }
    }

    std::vector<size_t> primGroups(numPrims);
    std::vector<size_t> groupCounts(numGroups, 0);
    for (size_t prim = 0; prim < numPrims; ++prim) {
        const auto groupIt = faceToGroup.find(primFaceIndices[prim]);
        primGroups[prim] =
            groupIt == faceToGroup.end() ? remainderGroup : groupIt->second;
        groupCounts[primGroups[prim]]++;
    }

    std::vector<size_t> groupOffsets(numGroups, 0);
    for (size_t group = 1; group < numGroups; ++group) {
        groupOffsets[group] = groupOffsets[group - 1] + groupCounts[group - 1];
    }

    // Order the primitives by group, stably: primOrder[newPrim] = oldPrim.
    VtIntArray primOrder(numPrims);
    bool identity = true;
    {
        std::vector<size_t> next = groupOffsets;
        for (size_t prim = 0; prim < numPrims; ++prim) {
            const size_t pos = next[primGroups[prim]]++;
            primOrder[pos] = (int)prim;
            identity = identity && (pos == prim);
        }
    }

    if (!identity) {
        // Reorder everything that is parallel to the primitives (one entry
        // per primitive) or to their corners. Data addressed through the
        // reordered buffers stays put: points and vertex/varying primvars
        // are indexed by the values of faceVertexIndices, and uniform
        // primvars stay in authored face order, addressed by decoding the
        // reordered primitiveParam.
        VtIntArray cornerOrder(numCorners);
        for (size_t prim = 0; prim < numPrims; ++prim) {
            const size_t oldFirstCorner = primOrder[prim] * cornersPerPrim;
            for (size_t corner = 0; corner < cornersPerPrim; ++corner) {
                cornerOrder[prim * cornersPerPrim + corner] =
                    (int)(oldFirstCorner + corner);
            }
        }

        VtIntArray sortedIndices(numCorners);
        const int* srcIndices = meshData->faceVertexIndices.cdata();
        for (size_t corner = 0; corner < numCorners; ++corner) {
            sortedIndices[corner] = srcIndices[cornerOrder[corner]];
        }
        meshData->faceVertexIndices = sortedIndices;

        meshData->primitiveParam =
            _GatherElements(meshData->primitiveParam, primOrder);

        if (!meshData->edgeIndices.IsEmpty()) {
            if (meshData->edgeIndices.GetArraySize() == numPrims) {
                meshData->edgeIndices =
                    _GatherElements(meshData->edgeIndices, primOrder);
            } else {
                TF_WARN("Edge indices of %s are not per-primitive; dropping "
                        "them from the draw-grouped copy",
                        meshData->id.GetText());
                meshData->edgeIndices = VtValue();
            }
        }

        // Refined face-varying channels hold one index entry per patch.
        // Their compact value buffers are addressed through these indices
        // and stay put.
        std::vector<TfToken> channelPrimvars;
        for (HydraPassthroughRenderData::FaceVaryingChannel& channel :
                meshData->faceVaryingChannels) {
            channelPrimvars.insert(channelPrimvars.end(),
                                   channel.primvars.begin(),
                                   channel.primvars.end());
            if (channel.indices.GetArraySize() == numPrims) {
                channel.indices = _GatherElements(channel.indices, primOrder);
            } else {
                TF_WARN("Face-varying channel %d of %s has indices that "
                        "don't match the mesh's primitives; its primvars "
                        "will misalign with the draw groups",
                        channel.channel, meshData->id.GetText());
            }
        }

        // Unrefined face-varying primvars have no channel and are already
        // per-corner, so their values move with the corners.
        for (auto& primvarEntry : meshData->primvars) {
            HydraPassthroughRenderData::PrimvarData& primvar =
                primvarEntry.second;
            if (primvar.interpolation != HdInterpolationFaceVarying ||
                std::find(channelPrimvars.begin(), channelPrimvars.end(),
                          primvarEntry.first) != channelPrimvars.end()) {
                continue;
            }
            if (primvar.data.GetArraySize() != numCorners) {
                TF_WARN("Face-varying primvar %s of %s has %zu values, "
                        "expected %zu; it will misalign with the draw "
                        "groups",
                        primvarEntry.first.GetText(),
                        meshData->id.GetText(),
                        primvar.data.GetArraySize(), numCorners);
                continue;
            }
            primvar.data = _GatherElements(primvar.data, cornerOrder);
        }
    }

    // Emit the groups in index elements. Subsets that ended up with no
    // primitives (e.g. all their faces are holes) produce no group.
    for (size_t group = 0; group < numGroups; ++group) {
        if (groupCounts[group] == 0) {
            continue;
        }
        meshData->drawGroups.push_back(
            {group == remainderGroup
                 ? meshData->materialId
                 : meshData->geomSubsets[group].materialId,
             (int)(groupOffsets[group] * cornersPerPrim),
             (int)(groupCounts[group] * cornersPerPrim)});
    }
}

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

void
WeldMesh(HydraPassthroughRenderData::MeshData* meshData)
{
    const VtIntArray vertexIndices = meshData->faceVertexIndices;
    const size_t numCorners = vertexIndices.size();
    if (numCorners == 0) {
        return;
    }

    // Each column maps corners to indices into some source value array.
    // Together the columns form the weld key of a corner: two corners weld
    // into one output vertex only if every column agrees.
    struct _Column {
        VtIntArray cornerIndices;
        // The primvars whose data the cornerIndices index into.
        std::vector<TfToken> primvars;
    };
    std::vector<_Column> columns;

    // The vertex column drives points and the vertex/varying primvars.
    // It is always present, so corners on different vertices never weld.
    const size_t vertexColumn = 0;
    columns.push_back({vertexIndices, {}});

    // Sort the primvars into the column layout by interpolation.
    std::vector<TfToken> fvarPrimvars;
    std::vector<TfToken> uniformPrimvars;
    for (const auto& primvarEntry : meshData->primvars) {
        switch (primvarEntry.second.interpolation) {
            case HdInterpolationVertex:
            case HdInterpolationVarying:
                columns[vertexColumn].primvars.push_back(primvarEntry.first);
                break;
            case HdInterpolationFaceVarying:
                fvarPrimvars.push_back(primvarEntry.first);
                break;
            case HdInterpolationUniform:
                uniformPrimvars.push_back(primvarEntry.first);
                break;
            default:
                // Constant and other interpolations are unaffected.
                break;
        }
    }

    // Refined face-varying primvars weld by their channel's indices, which
    // map corners into the compact value buffers.
    for (const HydraPassthroughRenderData::FaceVaryingChannel& channel :
            meshData->faceVaryingChannels) {
        _Column column;
        const bool validIndices = _FlattenFvarIndices(
            channel.indices, numCorners, &column.cornerIndices);
        if (!validIndices) {
            TF_WARN("Face-varying channel %d of %s has indices that don't "
                    "match the mesh's triangulated topology; leaving its "
                    "primvars unwelded",
                    channel.channel, meshData->id.GetText());
        }
        for (const TfToken& name : channel.primvars) {
            // Channel primvars are not per-corner, so exclude them from the
            // per-corner handling below whether or not we can weld them.
            const auto namePos =
                std::find(fvarPrimvars.begin(), fvarPrimvars.end(), name);
            if (namePos == fvarPrimvars.end()) {
                continue;
            }
            fvarPrimvars.erase(namePos);
            if (validIndices) {
                column.primvars.push_back(name);
            }
        }
        if (!column.primvars.empty()) {
            columns.push_back(std::move(column));
        }
    }

    // Unrefined face-varying primvars are already per-corner; weld them on
    // exact value equality.
    for (const TfToken& name : fvarPrimvars) {
        const HydraPassthroughRenderData::PrimvarData& primvar =
            meshData->primvars.find(name)->second;
        if (primvar.data.GetArraySize() != numCorners) {
            TF_WARN("Face-varying primvar %s of %s has %zu values, "
                    "expected %zu; leaving it unwelded",
                    name.GetText(), meshData->id.GetText(),
                    primvar.data.GetArraySize(), numCorners);
            continue;
        }
        VtIntArray column = VtVisitValue(
            primvar.data, _BuildFirstOccurrenceColumnVisitor());
        if (column.size() != numCorners) {
            TF_WARN("Could not weld face-varying primvar %s of %s",
                    name.GetText(), meshData->id.GetText());
            continue;
        }
        columns.push_back({std::move(column), {name}});
    }

    // Uniform primvars share one coarse-face column.
    if (!uniformPrimvars.empty()) {
        _Column column;
        if (_BuildUniformCornerIndices(meshData->primitiveParam, numCorners,
                                       &column.cornerIndices)) {
            column.primvars = std::move(uniformPrimvars);
            columns.push_back(std::move(column));
        } else {
            TF_WARN("Could not map faces to corners for %s; leaving uniform "
                    "primvars unwelded",
                    meshData->id.GetText());
        }
    }

    // Weld: assign an output vertex to each distinct weld key, in order of
    // first occurrence, and rewrite the index buffer.
    std::unordered_map<std::vector<int>, int, _CornerKeyHash> outputVertexIds;
    outputVertexIds.reserve(numCorners);
    VtIntArray newIndices(numCorners);
    std::vector<int> representativeCorners;
    representativeCorners.reserve(numCorners);
    std::vector<int> key(columns.size());
    for (size_t corner = 0; corner < numCorners; ++corner) {
        for (size_t i = 0; i < columns.size(); ++i) {
            key[i] = columns[i].cornerIndices.cdata()[corner];
        }
        const auto insertion = outputVertexIds.emplace(
            key, (int)representativeCorners.size());
        if (insertion.second) {
            representativeCorners.push_back((int)corner);
        }
        newIndices[corner] = insertion.first->second;
    }
    const size_t numOutputVertices = representativeCorners.size();

    // Rebuild each column's primvars with one value per output vertex,
    // gathered through the column at each vertex's representative corner.
    for (size_t columnIndex = 0; columnIndex < columns.size(); ++columnIndex) {
        const _Column& column = columns[columnIndex];

        VtIntArray remap(numOutputVertices);
        const int* cornerIndicesData = column.cornerIndices.cdata();
        for (size_t vertex = 0; vertex < numOutputVertices; ++vertex) {
            remap[vertex] = cornerIndicesData[representativeCorners[vertex]];
        }

        for (const TfToken& name : column.primvars) {
            HydraPassthroughRenderData::PrimvarData& primvar =
                meshData->primvars.find(name)->second;
            const VtValue welded = _GatherElements(primvar.data, remap);
            if (welded.IsEmpty()) {
                TF_WARN("Could not weld primvar %s of %s",
                        name.GetText(), meshData->id.GetText());
                continue;
            }
            primvar.data = welded;
            // The data is now parallel to points and addressed by
            // faceVertexIndices, which is vertex interpolation.
            primvar.interpolation = HdInterpolationVertex;
        }

        // The points member mirrors the points primvar; keep them
        // consistent.
        if (columnIndex == vertexColumn && !meshData->points.IsEmpty()) {
            const VtValue welded = _GatherElements(meshData->points, remap);
            if (!welded.IsEmpty()) {
                meshData->points = welded;
            }
        }
    }

    meshData->faceVertexIndices = newIndices;
    meshData->faceVaryingChannels.clear();
}

} // namespace HydraPassthroughMeshPackagingUtil

PXR_NAMESPACE_CLOSE_SCOPE
