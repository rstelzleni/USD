#include "normalComputations.h"

#include "pxr/imaging/hd/perfLog.h"
#include "pxr/imaging/hd/smoothNormals.h"
#include "pxr/imaging/hd/tokens.h"
#include "pxr/imaging/hd/vertexAdjacency.h"
#include "pxr/imaging/hd/vtBufferSource.h"

#include "pxr/imaging/hf/perfLog.h"

#include "pxr/base/vt/array.h"

#include "pxr/base/gf/vec3d.h"
#include "pxr/base/gf/vec3f.h"
#include "pxr/base/tf/token.h"

PXR_NAMESPACE_OPEN_SCOPE

// ---------------------------------------------------------------------------
// HydraPassthroughFlatNormalsComputationCPU
//
HydraPassthroughFlatNormalsComputationCPU::HydraPassthroughFlatNormalsComputationCPU(
    HdMeshTopology const * topology,
    HdBufferSourceSharedPtr const &points,
    TfToken const &dstName,
    bool packed)
    : _topology(topology)
    , _points(points)
    , _dstName(dstName)
    , _packed(packed)
{
}

void
HydraPassthroughFlatNormalsComputationCPU::GetBufferSpecs(HdBufferSpecVector *specs) const
{
    // The datatype of normals is the same as that of points, unless the
    // packed format was requested.
    specs->emplace_back(_dstName,
        _packed
            ? HdTupleType { HdTypeInt32_2_10_10_10_REV, 1 }
            : _points->GetTupleType() );
}

TfToken const &
HydraPassthroughFlatNormalsComputationCPU::GetName() const
{
    return _dstName;
}

bool
HydraPassthroughFlatNormalsComputationCPU::Resolve()
{
    if (!_points->IsResolved()) { return false; }
    if (!_TryLock()) { return false; }

    HD_TRACE_FUNCTION();
    HF_MALLOC_TAG_FUNCTION();

    if (!TF_VERIFY(_topology)) return true;

    VtValue normals;

    switch (_points->GetTupleType().type) {
    case HdTypeFloatVec3:
        if (_packed) {
            normals = Hd_FlatNormals::ComputeFlatNormalsPacked(
                _topology, static_cast<const GfVec3f*>(_points->GetData()));
        } else {
            normals = Hd_FlatNormals::ComputeFlatNormals(
                _topology, static_cast<const GfVec3f*>(_points->GetData()));
        }
        break;
    case HdTypeDoubleVec3:
        if (_packed) {
            normals = Hd_FlatNormals::ComputeFlatNormalsPacked(
                _topology, static_cast<const GfVec3d*>(_points->GetData()));
        } else {
            normals = Hd_FlatNormals::ComputeFlatNormals(
                _topology, static_cast<const GfVec3d*>(_points->GetData()));
        }
        break;
    default:
        TF_CODING_ERROR("Unsupported points type for computing flat normals");
        break;
    }

    HdBufferSourceSharedPtr normalsBuffer = HdBufferSourceSharedPtr(
        new HdVtBufferSource(_dstName, VtValue(normals)));
    _SetResult(normalsBuffer);
    _SetResolved();
    return true;
}

bool
HydraPassthroughFlatNormalsComputationCPU::_CheckValid() const
{
    bool valid = _points ? _points->IsValid() : false;
    return valid;
}

// ---------------------------------------------------------------------------
// HydraPassthroughSmoothNormalsComputationCPU
//
HydraPassthroughSmoothNormalsComputationCPU::HydraPassthroughSmoothNormalsComputationCPU(
    Hd_VertexAdjacency const *adjacency,
    HdBufferSourceSharedPtr const &points,
    TfToken const &dstName,
    HdBufferSourceSharedPtr const &adjacencyBuilder,
    bool packed)
    : _adjacency(adjacency)
    , _points(points)
    , _dstName(dstName)
    , _adjacencyBuilder(adjacencyBuilder)
    , _packed(packed)
{
}

void
HydraPassthroughSmoothNormalsComputationCPU::GetBufferSpecs(
    HdBufferSpecVector *specs) const
{
    // The datatype of normals is the same as that of points,
    // unless the packed format was requested.
    specs->emplace_back(_dstName,
        _packed
            ? HdTupleType { HdTypeInt32_2_10_10_10_REV, 1 }
            : _points->GetTupleType() );
}

TfToken const &
HydraPassthroughSmoothNormalsComputationCPU::GetName() const
{
    return _dstName;
}

bool
HydraPassthroughSmoothNormalsComputationCPU::Resolve()
{
    // dependency check first
    if (_adjacencyBuilder) {
        if (!_adjacencyBuilder->IsResolved()) return false;
    }
    if (!_points->IsResolved()) return false;
    if (!_TryLock()) return false;

    HD_TRACE_FUNCTION();
    HF_MALLOC_TAG_FUNCTION();

    if (!TF_VERIFY(_adjacency)) return true;

    size_t numPoints = _points->GetNumElements();

    HdBufferSourceSharedPtr normals;

    switch (_points->GetTupleType().type) {
    case HdTypeFloatVec3:
        if (_packed) {
            normals = HdBufferSourceSharedPtr(
                new HdVtBufferSource(
                    _dstName, VtValue(
                        Hd_SmoothNormals::ComputeSmoothNormalsPacked(
                            _adjacency,
                            numPoints,
                            static_cast<const GfVec3f*>(_points->GetData())))));
        } else {
            normals = HdBufferSourceSharedPtr(
                new HdVtBufferSource(
                    _dstName, VtValue(
                        Hd_SmoothNormals::ComputeSmoothNormals(
                            _adjacency,
                            numPoints,
                            static_cast<const GfVec3f*>(_points->GetData())))));
        }
        break;
    case HdTypeDoubleVec3:
        if (_packed) {
            normals = HdBufferSourceSharedPtr(
                new HdVtBufferSource(
                    _dstName, VtValue(
                        Hd_SmoothNormals::ComputeSmoothNormalsPacked(
                            _adjacency,
                            numPoints,
                            static_cast<const GfVec3d*>(_points->GetData())))));
        } else {
            normals = HdBufferSourceSharedPtr(
                new HdVtBufferSource(
                    _dstName, VtValue(
                        Hd_SmoothNormals::ComputeSmoothNormals(
                            _adjacency,
                            numPoints,
                            static_cast<const GfVec3d*>(_points->GetData())))));
        }
        break;
    default:
        TF_CODING_ERROR("Unsupported points type for computing smooth normals");
        break;
    }

    _SetResult(normals);

    // call base class to mark as resolved.
    _SetResolved();
    return true;
}

bool
HydraPassthroughSmoothNormalsComputationCPU::_CheckValid() const
{
    bool valid = _points ? _points->IsValid() : false;

    // _adjacencyBuilder is an optional source
    valid &= _adjacencyBuilder ? _adjacencyBuilder->IsValid() : true;

    return valid;
}


PXR_NAMESPACE_CLOSE_SCOPE
