#include "triangulate.h"

#include "pxr/usdImaging/hydraPassthrough/meshTopology.h"
#include "pxr/imaging/hd/meshUtil.h"
#include "pxr/imaging/hd/perfLog.h"
#include "pxr/imaging/hd/tokens.h"
#include "pxr/imaging/hd/vtBufferSource.h"

#include "pxr/base/gf/vec3i.h"

PXR_NAMESPACE_OPEN_SCOPE

HydraPassthroughTriangleIndexBuilderComputation::HydraPassthroughTriangleIndexBuilderComputation(
    HydraPassthroughMeshTopology *topology, SdfPath const &id)
    : _id(id), _topology(topology)
{
}

void
HydraPassthroughTriangleIndexBuilderComputation::GetBufferSpecs(
    HdBufferSpecVector *specs) const
{
    specs->emplace_back(HdTokens->indices, HdTupleType{HdTypeInt32Vec3, 1});
    // triangles don't support ptex indexing (at least for now).
    specs->emplace_back(HdTokens->primitiveParam,
                        HdTupleType{HdTypeInt32, 1});
    // 1 edge index per triangle
    specs->emplace_back(HdTokens->edgeIndices,
                        HdTupleType{HdTypeInt32, 1});
}

bool
HydraPassthroughTriangleIndexBuilderComputation::Resolve()
{
    if (!_TryLock()) return false;

    HD_TRACE_FUNCTION();

    VtVec3iArray trianglesFaceVertexIndices;
    VtIntArray primitiveParam;
    VtIntArray trianglesEdgeIndices;

    HdMeshTopology* hdTopology = dynamic_cast<HdMeshTopology*>(_topology);

    HdMeshUtil meshUtil(hdTopology, _id);
    meshUtil.ComputeTriangleIndices(
            &trianglesFaceVertexIndices,
            &primitiveParam,
            &trianglesEdgeIndices);

    _SetResult(std::make_shared<HdVtBufferSource>(
                       HdTokens->indices,
                       VtValue(trianglesFaceVertexIndices)));

    _primitiveParam.reset(new HdVtBufferSource(
                              HdTokens->primitiveParam,
                              VtValue(primitiveParam)));

    _trianglesEdgeIndices.reset(new HdVtBufferSource(
                                   HdTokens->edgeIndices,
                                   VtValue(trianglesEdgeIndices)));

    _SetResolved();
    return true;
}

bool
HydraPassthroughTriangleIndexBuilderComputation::HasChainedBuffer() const
{
    return true;
}

HdBufferSourceSharedPtrVector
HydraPassthroughTriangleIndexBuilderComputation::GetChainedBuffers() const
{
    return { _primitiveParam, _trianglesEdgeIndices };
}

bool
HydraPassthroughTriangleIndexBuilderComputation::_CheckValid() const
{
    return true;
}

// ---------------------------------------------------------------------------

HydraPassthroughTriangulateFaceVaryingComputation::HydraPassthroughTriangulateFaceVaryingComputation(
    HydraPassthroughMeshTopology *topology,
    HdBufferSourceSharedPtr const &source,
    SdfPath const &id)
    : _id(id), _topology(topology), _source(source)
{
}

bool
HydraPassthroughTriangulateFaceVaryingComputation::Resolve()
{
    if (!TF_VERIFY(_source)) return false;
    if (!_source->IsResolved()) return false;

    if (!_TryLock()) return false;

    HD_TRACE_FUNCTION();
    HD_PERF_COUNTER_INCR(HdPerfTokens->triangulateFaceVarying);

    VtValue result;
    HdMeshUtil meshUtil(_topology, _id);
    if(meshUtil.ComputeTriangulatedFaceVaryingPrimvar(
            _source->GetData(),
            _source->GetNumElements(),
            _source->GetTupleType().type,
            &result)) {
        _SetResult(std::make_shared<HdVtBufferSource>(
                        _source->GetName(),
                        result));
    } else {
        _SetResult(_source);
    }

    _SetResolved();
    return true;
}

void
HydraPassthroughTriangulateFaceVaryingComputation::GetBufferSpecs(HdBufferSpecVector *specs) const
{
    // produces same spec buffer as source
    _source->GetBufferSpecs(specs);
}

bool
HydraPassthroughTriangulateFaceVaryingComputation::_CheckValid() const
{
    return (_source->IsValid());
}

PXR_NAMESPACE_CLOSE_SCOPE

