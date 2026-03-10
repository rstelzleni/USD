#include "pxr/usdImaging/hydraPassthrough/vertexAdjacency.h"

#include "pxr/imaging/hd/meshTopology.h"
#include "pxr/imaging/hd/tokens.h"
#include "pxr/imaging/hd/vtBufferSource.h"

#include "pxr/imaging/hd/perfLog.h"
#include "pxr/imaging/hf/perfLog.h"

PXR_NAMESPACE_OPEN_SCOPE

namespace {

// ---------------------------------------------------------------------------
// _VertexAdjacencyBufferSource 
//
// A buffer source that puts an already computed adjacency table into
// a resource registry buffer. This computation should be dependent on an
// HydraPassthroughVertexAdjacencyBuilderComputation.
class _VertexAdjacencyBufferSource : public HdComputedBufferSource
{
public:
    _VertexAdjacencyBufferSource(
        Hd_VertexAdjacency const *vertexAdjacency,
        HdBufferSourceSharedPtr const &vertexAdjacencyBuilder);

    void GetBufferSpecs(HdBufferSpecVector *specs) const override;

    bool Resolve() override;

protected:
    bool _CheckValid() const override;

private:
    Hd_VertexAdjacency const *_vertexAdjacency;
    HdBufferSourceSharedPtr const _vertexAdjacencyBuilder;
};

_VertexAdjacencyBufferSource::_VertexAdjacencyBufferSource(
    Hd_VertexAdjacency const *vertexAdjacency,
    HdBufferSourceSharedPtr const &vertexAdjacencyBuilder)
    : _vertexAdjacency(vertexAdjacency)
    , _vertexAdjacencyBuilder(vertexAdjacencyBuilder)
{
}

bool
_VertexAdjacencyBufferSource::Resolve()
{
    if (!_vertexAdjacencyBuilder->IsResolved()) return false;
    if (!_TryLock()) return false;

    HD_TRACE_FUNCTION();
    HF_MALLOC_TAG_FUNCTION();

    // prepare buffer source to be transferred.
    VtIntArray const &vertexAdjacency = _vertexAdjacency->GetAdjacencyTable();
    _SetResult(std::make_shared<HdVtBufferSource>(
                        HdTokens->adjacency, VtValue(vertexAdjacency)));
    _SetResolved();
    return true;
}

void
_VertexAdjacencyBufferSource::GetBufferSpecs(
    HdBufferSpecVector *specs) const
{
    specs->emplace_back(HdTokens->adjacency, HdTupleType{HdTypeInt32, 1});
}

bool
_VertexAdjacencyBufferSource::_CheckValid() const
{
    return true;
}


} // anonymous namespace

// ---------------------------------------------------------------------------
// HydraPassthroughVertexAdjacencyBuilderComputation
HydraPassthroughVertexAdjacencyBuilderComputation::HydraPassthroughVertexAdjacencyBuilderComputation(
    Hd_VertexAdjacency *vertexAdjacency,
    HdMeshTopology const *topology)
    : _vertexAdjacency(vertexAdjacency)
    , _topology(topology)
{
}

bool
HydraPassthroughVertexAdjacencyBuilderComputation::Resolve()
{
    if (!_TryLock()) return false;

    HD_TRACE_FUNCTION();
    HF_MALLOC_TAG_FUNCTION();

    _vertexAdjacency->BuildAdjacencyTable(_topology);

    // call base class to mark as resolved.
    _SetResolved();
    return true;
}

bool
HydraPassthroughVertexAdjacencyBuilderComputation::_CheckValid() const
{
    return true;
}

// ---------------------------------------------------------------------------
// HydraPassthroughVertexAdjacencyBuilder implementation

HdBufferSourceSharedPtr
HydraPassthroughVertexAdjacencyBuilder::GetSharedVertexAdjacencyBuilderComputation(
    HdMeshTopology const *topology)
{
    // Quick implemenation note about this memory scheme specifically.
    //
    // It seems like we're using a shared_ptr to manage the lifetime of the builder
    // computation, but we're storing it as a weak_ptr in this class. This means
    // that the builder computation will be automatically destroyed when there
    // are no more external shared_ptr references to it.
    //
    // This way when we render we'll create one of these, and all requests will
    // return that one. Once the returned shared_ptrs are released this class
    // will not have a reference to the builder computation, but it will still
    // have the _vertexAdjacency data it computed. So callers could still
    // access this after the computations are complete and freed.
    //
    // If a new request comes in to this function in that state, we'll return
    // a computation, but that computation won't run until we Commit, so there's
    // a window where the _vertexAdjacency is stale. I don't think I'd have
    // designed it this way, but I'm keeping it because this is how it works
    // in HdSt_VertexAdjacencyBuilder.

    // if there's a already requested (and unresolved) adjacency computation,
    // just returns it to make a dependency.
    if (std::shared_ptr<HydraPassthroughVertexAdjacencyBuilderComputation> builder =
        _sharedVertexAdjacencyBuilder.lock()) {

        return builder;
    }

    // if cpu adjacency table exists, no need to compute again
    if (!(_vertexAdjacency.GetAdjacencyTable().empty())) {
        return nullptr;
    }

    std::shared_ptr<HydraPassthroughVertexAdjacencyBuilderComputation> builder =
        std::make_shared<HydraPassthroughVertexAdjacencyBuilderComputation>(
            &_vertexAdjacency, topology);

    // store the computation as weak ptr so that it can be referenced
    // by another computation.
    _sharedVertexAdjacencyBuilder = builder;

    return builder;
}

HdBufferSourceSharedPtr
HydraPassthroughVertexAdjacencyBuilder::GetVertexAdjacencyBufferSource()
{
    if (std::shared_ptr<HydraPassthroughVertexAdjacencyBuilderComputation> builder =
        _sharedVertexAdjacencyBuilder.lock()) {

        return std::make_shared<_VertexAdjacencyBufferSource>(
            &_vertexAdjacency, builder);
    }

    TF_CODING_ERROR("No adjacency builder computation found. Call "
                    "GetSharedVertexAdjacencyBuilderComputation first to create "
                    "a computation.");
    return nullptr;
}

PXR_NAMESPACE_CLOSE_SCOPE

