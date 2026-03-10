#ifndef USD_IMAGING_HYDRA_PASSTHROUGH_VERTEX_ADJACENCY_H
#define USD_IMAGING_HYDRA_PASSTHROUGH_VERTEX_ADJACENCY_H

#include "pxr/pxr.h"

#include "pxr/imaging/hd/bufferArrayRange.h"
#include "pxr/imaging/hd/bufferSource.h"
#include "pxr/imaging/hd/vertexAdjacency.h"

#include "pxr/base/vt/array.h"

#include <memory>

PXR_NAMESPACE_OPEN_SCOPE


class HdMeshTopology;
class HydraPassthroughVertexAdjacencyBuilderComputation;

/// \class HydraPassthroughVertexAdjacencyBuilder
///
/// Manages data related to computing a Hd_VertexAdjacency, and owns
/// the Hd_VertexAdjacency itself.
///
/// This is intended to be cached by topology hash, and will allow
/// reuse of adjacency tables.
class HydraPassthroughVertexAdjacencyBuilder final
{
public:

    Hd_VertexAdjacency const *GetVertexAdjacency() const {
        return &_vertexAdjacency;
    }

    /// Returns a shared adjacency builder computation which will call
    /// BuildAdjacencyTable.  The shared computation is useful if multiple
    /// meshes share a topology and adjacency table, and only want to build the
    /// adjacency table once.
    HdBufferSourceSharedPtr GetSharedVertexAdjacencyBuilderComputation(
        HdMeshTopology const *topology);

    /// Gets a buffer source for the adjacency table. This will have
    /// the build computation as a dependency.
    ///
    /// Returns nullptr if there is no builder computation yet,
    /// GetSharedVertexAdjacencyBuilderComputation should be called first
    HdBufferSourceSharedPtr GetVertexAdjacencyBufferSource();

private:
    Hd_VertexAdjacency _vertexAdjacency;

    std::weak_ptr<HydraPassthroughVertexAdjacencyBuilderComputation>
        _sharedVertexAdjacencyBuilder;

};

/// \class HydraPassthroughVertexAdjacencyBuilderComputation
///
/// A null buffer source to compute the adjacency table. Since this is a null
/// buffer source, it won't actually produce buffer output; but other
/// computations can depend on this to ensure BuildAdjacencyTable is called.
///
/// The main purpose of separating the builder and buffer source is that
/// multiple buffer sources can depend on the same builder, in the event of
/// matching topologies.
///
/// When these are reused, a shared pointer to the same computation is returned
/// from the Builder. That single computation is then added to the resource
/// registry multiple times. The resource registry will call IsResolved, then
/// Resolve on each of them, but since the first one should resolve the
/// computation, the rest will not run. This is how it works in HdSt, so I'm
/// sticking with it for now. In the future once we have a working system this
/// could be refactored.
class HydraPassthroughVertexAdjacencyBuilderComputation : public HdNullBufferSource
{
public:
    HydraPassthroughVertexAdjacencyBuilderComputation(
            Hd_VertexAdjacency *vertexAdjacency,
            HdMeshTopology const *topology);
    bool Resolve() override;

protected:
    bool _CheckValid() const override;

private:
    Hd_VertexAdjacency *_vertexAdjacency;
    HdMeshTopology const *_topology;
};


PXR_NAMESPACE_CLOSE_SCOPE


#endif // USD_IMAGING_HYDRA_PASSTHROUGH_VERTEX_ADJACENCY_H
