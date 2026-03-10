#ifndef USD_IMAGING_HYDRA_PASSTHROUGH_RESOURCE_REGISTRY_H
#define USD_IMAGING_HYDRA_PASSTHROUGH_RESOURCE_REGISTRY_H

#include "pxr/pxr.h"

#include "pxr/imaging/hd/bufferArrayRange.h"
#include "pxr/imaging/hd/bufferSource.h"
#include "pxr/imaging/hd/instanceRegistry.h"
#include "pxr/imaging/hd/perfLog.h"
#include "pxr/imaging/hd/resourceRegistry.h"
#include "pxr/imaging/hd/types.h"
#include "pxr/usd/sdf/path.h"
#include "pxr/base/tf/declarePtrs.h"

#include <atomic>

#include <tbb/concurrent_vector.h>

PXR_NAMESPACE_OPEN_SCOPE

TF_DECLARE_REF_PTRS(HydraPassthroughRenderData);

class HydraPassthroughVertexAdjacencyBuilder;

/// \class HydraPassthroughResourceRegistry
class HydraPassthroughResourceRegistry : public HdResourceRegistry {
public:

    /// Type of computed primvar source
    enum class PrimvarSourceType {
        Index,
        Primvar,
        Generic
    };

public:
    HydraPassthroughResourceRegistry();
    ~HydraPassthroughResourceRegistry() = default;

    /// Sets the render data to copy output into.
    ///
    /// Must be called before Commit, so there is a location to send the output
    void SetRenderData(HydraPassthroughRenderDataRefPtr const &renderData) {
        _renderData = renderData;
    }

    /// Append a computation for Index data
    void AddIndexSources(SdfPath const &id,
                         HdBufferSourceSharedPtrVector &&sources);

    // Append a computation for a Primvar
    void AddPrimvarSource(SdfPath const &id,
                          HdBufferSourceSharedPtr const &source,
                          HdInterpolation interpolation);

    /// Append a list of primvar computations
    void AddPrimvarSources(SdfPath const &id,
                           HdBufferSourceSharedPtrVector &&sources,
                           HdInterpolation interpolation);

    /// Append a source data just to be resolved (used for cpu computations).
    void AddGenericSource(SdfPath const &id,
                          HdBufferSourceSharedPtr const &source);

    // -------------------------------------------------------------------
    // HdInstanceRegistry accessors
    //
    // These registries implement sharing and deduplication of data based
    // on computed hash identifiers. Each returned HdInstance object retains
    // a shared pointer to a data instance. When an HdInstance is registered
    // for a previously unused ID, the data pointer will be null and it is
    // the caller's responsibility to set its value. The instance registries
    // are cleaned of unreferenced entries during garbage collection.
    //
    // Note: As entries can be registered from multiple threads, the returned
    // object holds a lock on the instance registry. This lock is held
    // until the returned HdInstance object is destroyed.

    /// Gets a vertex adjacency builder computation for a topology id
    HdInstance<std::shared_ptr<HydraPassthroughVertexAdjacencyBuilder>>
    RegisterVertexAdjacencyBuilder(
        HdInstance<std::shared_ptr<HydraPassthroughVertexAdjacencyBuilder>>::ID id);

protected:

    // Kicks off parallel computations
    void _Commit() override;

    // Cleans up resources post render
    //
    // This seems a litle goofy, but is the design. It seems weird to create
    // objects like topology builders in another class then destroy them here.
    void _GarbageCollect() override;

private:
    // See this TODO below, which I copied from HdStResourceRegistry. This
    // comment is from 2020, so I wonder if this will still happen. But,
    // keeping it here for breadcrumbs in the future.
    //
    // TODO: this is a transient structure. we'll revisit the BufferSource
    // interface later.
    struct _PendingSource {
        _PendingSource(HdBufferSourceSharedPtr const &source,
                       SdfPath const &id,
                       PrimvarSourceType type,
                       HdInterpolation interpolation,
                       bool isIntermediate = false)
            : type(type)
            , sources(1, source)
            , id(id)
            , interpolation(interpolation)
            , isIntermediate(isIntermediate)
        {
        }
        
        _PendingSource(HdBufferSourceSharedPtrVector &&sources,
                       SdfPath const &id,
                       PrimvarSourceType type,
                       HdInterpolation interpolation,
                       bool isIntermediate = false)
            : type(type)
            , sources(std::move(sources))
            , id(id)
            , interpolation(interpolation)
            , isIntermediate(isIntermediate)
        {
        }

        //HdBufferArrayRangeSharedPtr range;
        PrimvarSourceType type { PrimvarSourceType::Generic };
        HdBufferSourceSharedPtrVector sources;
        SdfPath id;

        // Will only be set for primvar sources
        HdInterpolation interpolation { HdInterpolation::HdInterpolationCount };

        // True for buffers that are inputs, for instance, unsubdivided points in a
        // subdivision surface
        bool isIntermediate { false };
    };

    void _AddSource(HdBufferSourceSharedPtr const &source,
                    SdfPath const &path,
                    PrimvarSourceType type,
                    HdInterpolation interpolation = HdInterpolation::HdInterpolationCount,
                    bool isIntermediate = false);
    void _AddSources(HdBufferSourceSharedPtrVector &&sources,
                     SdfPath const &path,
                     PrimvarSourceType type,
                     HdInterpolation interpolation = HdInterpolation::HdInterpolationCount);

    typedef tbb::concurrent_vector<_PendingSource> _PendingSourceList;
    _PendingSourceList    _pendingSources;
    std::atomic_size_t    _numBufferSourcesToResolve;

    HydraPassthroughRenderDataRefPtr _renderData;

    // HdInstanceRegistry objects (caches)
    HdInstanceRegistry<std::shared_ptr<HydraPassthroughVertexAdjacencyBuilder>>
        _vertexAdjacencyBuilderRegistry;
};

PXR_NAMESPACE_CLOSE_SCOPE

#endif // USD_IMAGING_HYDRA_PASSTHROUGH_RESOURCE_REGISTRY_H
