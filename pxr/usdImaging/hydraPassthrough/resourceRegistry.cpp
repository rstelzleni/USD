#include "resourceRegistry.h"
#include "renderData.h"
#include "vertexAdjacency.h"

#include "pxr/base/work/loops.h"
#include "pxr/imaging/hd/tokens.h"
#include "pxr/imaging/hd/vtBufferSource.h"
#include "pxr/usd/sdf/path.h"

#include <iostream>

PXR_NAMESPACE_OPEN_SCOPE

// --------------------------------------------------------------------
// Helpers for working with HdInstanceRegistry.
//
// You might see a name like HdInstance and think it has to do with instancing,
// but you'd be falling into Hydra's trap! It's actually a general-purpose cache
// for arbitrary classes, keyed by ids the caller tracks and passes in.
//
// This is primarily used for deduplicating topology data.

TF_DEFINE_ENV_SETTING(HYDRA_PASSTHROUGH_ENABLE_RESOURCE_INSTANCING, true,
                  "Enable instance registry deduplication of resource data");

static bool
_IsEnabledResourceInstancing()
{
    static bool isResourceInstancingEnabled =
        TfGetEnvSetting(HYDRA_PASSTHROUGH_ENABLE_RESOURCE_INSTANCING);
    return isResourceInstancingEnabled;
}

template <typename ID, typename T>
HdInstance<T>
_Register(ID id, HdInstanceRegistry<T> &registry, TfToken const &perfToken)
{
    if (_IsEnabledResourceInstancing()) {
        HdInstance<T> instance = registry.GetInstance(id);

        if (instance.IsFirstInstance()) {
            HD_PERF_COUNTER_INCR(perfToken);
        }

        return instance;
    } else {
        // Return an instance that is not managed by the registry when
        // topology instancing is disabled.
        return HdInstance<T>(id);
    }
}

// --------------------------------------------------------------------

static size_t
_GetChainedStagingSize(HdBufferSourceSharedPtr const& src)
{
    size_t size = 0;

    if (src->HasChainedBuffer()) {
        HdBufferSourceSharedPtrVector chainedSrcs = src->GetChainedBuffers();
        // Traverse the tree in a depth-first fashion.
        for (auto& c : chainedSrcs) {
            const size_t numElements = c->GetNumElements();
            if (numElements > 0) {
                size += numElements * HdDataSizeOfTupleType(c->GetTupleType());
            }
            size += _GetChainedStagingSize(c);
        }
    }

    return size;
}

HydraPassthroughResourceRegistry::HydraPassthroughResourceRegistry()
    : HdResourceRegistry()
    , _numBufferSourcesToResolve(0)
{
}


void HydraPassthroughResourceRegistry::AddIndexSources(
        SdfPath const &id,
        HdBufferSourceSharedPtrVector &&sources)
{
    _AddSources(std::move(sources), id, PrimvarSourceType::Index);
}

void HydraPassthroughResourceRegistry::AddPrimvarSource(
        SdfPath const &id,
        HdBufferSourceSharedPtr const &source,
        HdInterpolation interpolation,
        bool isIntermediate)
{
    _AddSource(source, id, PrimvarSourceType::Primvar, interpolation,
               isIntermediate);
}

void HydraPassthroughResourceRegistry::AddPrimvarSources(
        SdfPath const &id,
        HdBufferSourceSharedPtrVector &&sources,
        HdInterpolation interpolation)
{

    _AddSources(std::move(sources), id, PrimvarSourceType::Primvar, interpolation);
}

void HydraPassthroughResourceRegistry::AddGenericSource(
        SdfPath const &id,
        HdBufferSourceSharedPtr const &source)
{
    _AddSource(source, id, PrimvarSourceType::Generic);
}

void HydraPassthroughResourceRegistry::_AddSource(
        HdBufferSourceSharedPtr const &source,
        SdfPath const &id,
        HydraPassthroughResourceRegistry::PrimvarSourceType type,
        HdInterpolation interpolation,
        bool isIntermediate)
{
    if (!source || !source->IsValid())
    {
        TF_RUNTIME_ERROR("source pointer is null or invalid");
        return;
    }

    if (source->HasPreChainedBuffer()) {
        _AddSource(source->GetPreChainedBuffer(), id, type,
                   interpolation, /*isIntermediate=*/true);
    }

    _pendingSources.emplace_back(source, id, type, interpolation, isIntermediate);
    ++_numBufferSourcesToResolve; // Atomic
}

void HydraPassthroughResourceRegistry::_AddSources(
        HdBufferSourceSharedPtrVector &&sources,
        SdfPath const &id,
        HydraPassthroughResourceRegistry::PrimvarSourceType type,
        HdInterpolation interpolation)
{
    if (sources.empty())
    {
        TF_WARN("sources list is empty for id=%s, type=%d", id.GetText(), (int)type);
        return;
    }

    for (HdBufferSourceSharedPtr const &source : sources) {
        if (!source || !source->IsValid()) {
            TF_RUNTIME_ERROR("source pointer is null or invalid");
            continue;
        }

        if (source->HasPreChainedBuffer()) {
            _AddSource(source->GetPreChainedBuffer(), id, type,
                       interpolation, /*isIntermediate=*/true);
        }

        _pendingSources.emplace_back(source, id, type, interpolation);
        ++_numBufferSourcesToResolve; // Atomic
    }
}

void
HydraPassthroughResourceRegistry::_Commit()
{
    TF_STATUS("Committing %zu pending buffer sources", _pendingSources.size());

    {
        HD_TRACE_SCOPE("Resolve");

        std::atomic_size_t numBufferSourcesResolved { 0 };
        int numIterations = 0;

        // iterate until all buffer sources have been resolved.
        while (numBufferSourcesResolved < _numBufferSourcesToResolve) {

            // Reset the count to zero, we re-count all in case there are duplicates
            // in the list of pending sources, which happens with vertex adjacency
            // builder sources.
            numBufferSourcesResolved = 0;

            // iterate over all pending sources
            WorkParallelForEach(_pendingSources.begin(), _pendingSources.end(),
                [&numBufferSourcesResolved](_PendingSource &req) {
                    for (HdBufferSourceSharedPtr const& source: req.sources) {
                        if (source->IsResolved()) {
                            ++numBufferSourcesResolved;
                        } else {
                            if (source->Resolve()) {
                                TF_VERIFY(source->IsResolved(), 
                                "Name = %s", source->GetName().GetText());

                                ++numBufferSourcesResolved;
                            }
                        }
                    }
                });

            if (++numIterations > 100) {
                TF_WARN("Too many iterations in resolving buffer source. "
                        "It's likely due to inconsistent dependency.");
                break;
            }
        }

        TF_VERIFY(numBufferSourcesResolved == _numBufferSourcesToResolve);
    }

    {
        HD_TRACE_SCOPE("Copy");

        for (_PendingSource &pendingSource : _pendingSources) {

            // Skip topology objects, these are the generic sources, and hold
            // onto topology data that was intermediate. There's no data
            // to copy
            if (pendingSource.type == PrimvarSourceType::Generic) {
                continue;
            }

            // Skip intermediate sources. These are things like the rough,
            // unsubdivided points for a subdivision surface. They are inputs,
            // and there will be similarly named outputs that will contain the
            // final computed values.
            if (pendingSource.isIntermediate) {
                continue;
            }

            // Now extract the computed data
            for (HdBufferSourceSharedPtr const& src : pendingSource.sources) {
                // A source that failed to resolve (e.g. an ext computation
                // with no CPU callback) has no data to publish; its data
                // pointer may be null even though its declared tuple type
                // and element count look valid.
                if (src->HasResolveError()) {
                    TF_WARN("Skipping unresolved buffer source '%s' on <%s>",
                            src->GetName().GetText(),
                            pendingSource.id.GetText());
                    continue;
                }

                _renderData->CopyPrimvarBufferSource(
                        pendingSource.id,
                        src,
                        pendingSource.type,
                        pendingSource.interpolation);
            }
        }
    }

    // release sources
    WorkParallelForEach(_pendingSources.begin(), _pendingSources.end(),
                        [](_PendingSource &ps) {
                            ps.sources.clear();
                        });

    _pendingSources.clear();
    _numBufferSourcesToResolve = 0;
}

void
HydraPassthroughResourceRegistry::_GarbageCollect()
{
    {
        size_t count = _vertexAdjacencyBuilderRegistry.GarbageCollect();
        HD_PERF_COUNTER_SET(HdPerfTokens->instVertexAdjacency, count);
    }
}

HdInstance<std::shared_ptr<HydraPassthroughVertexAdjacencyBuilder>>
HydraPassthroughResourceRegistry::RegisterVertexAdjacencyBuilder(
        HdInstance<std::shared_ptr<HydraPassthroughVertexAdjacencyBuilder>>::ID id)
{
    return _Register(id, _vertexAdjacencyBuilderRegistry,
                     HdPerfTokens->instVertexAdjacency);
}


HdBufferSourceSharedPtr
HydraPassthroughResourceRegistry::GetPointsSource(SdfPath const &id) const
{
    // Look for a non-intermediate primvar source with the name "points"
    for (auto const& pendingSource: _pendingSources) {
        if (!pendingSource.isIntermediate &&
            pendingSource.type == PrimvarSourceType::Primvar &&
            pendingSource.id == id) {

            for (HdBufferSourceSharedPtr const& src : pendingSource.sources) {
                if (src->GetName() == HdTokens->points) {
                    return src;
                }
            }
        }
    }

    return nullptr;
}


PXR_NAMESPACE_CLOSE_SCOPE
