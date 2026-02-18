#include "resourceRegistry.h"
#include "renderData.h"

#include "pxr/base/work/loops.h"
#include "pxr/imaging/hd/tokens.h"
#include "pxr/imaging/hd/vtBufferSource.h"
#include "pxr/usd/sdf/path.h"

PXR_NAMESPACE_OPEN_SCOPE

static void
_CopyChainedBuffers(HdBufferSourceSharedPtr const&  src,
                    HdBufferArrayRangeSharedPtr const& range)
{
    if (src->HasChainedBuffer()) {
        HdBufferSourceSharedPtrVector chainedSrcs = src->GetChainedBuffers();
        // Traverse the tree in a depth-first fashion.
        for(auto& c : chainedSrcs) {
            range->CopyData(c);
            _CopyChainedBuffers(c, range);
        }
    }
}

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

void HydraPassthroughResourceRegistry::AddPointsSource(
        SdfPath const &id,
        HdBufferSourceSharedPtr const &source)
{
    _AddSource(source, id, PrimvarSourceType::Points);
}

void HydraPassthroughResourceRegistry::AddPrimvarSource(
        SdfPath const &id,
        HdBufferSourceSharedPtr const &source,
        HdInterpolation interpolation)
{
    _AddSource(source, id, PrimvarSourceType::Primvar, interpolation);
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
        HdInterpolation interpolation)
{
    if (!source || !source->IsValid())
    {
        TF_RUNTIME_ERROR("source pointer is null or invalid");
        return;
    }

    if (source->HasPreChainedBuffer()) {
        _AddSource(source->GetPreChainedBuffer(), id, type);
    }

    printf("SINGLE Adding source %s of type %d to pending sources\n", source->GetName().GetText(), (int)type);
    _pendingSources.emplace_back(source, id, type, interpolation);
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
        TF_RUNTIME_ERROR("sources list is empty");
        return;
    }

    for (HdBufferSourceSharedPtr const &source : sources) {
        if (!source || !source->IsValid()) {
            TF_RUNTIME_ERROR("source pointer is null or invalid");
            continue;
        }

        if (source->HasPreChainedBuffer()) {
            _AddSource(source->GetPreChainedBuffer(), id, type);
        }

        printf("NNN Adding source %s of type %d to pending sources\n", source->GetName().GetText(), (int)type);
        _pendingSources.emplace_back(source, id, type, interpolation);
        ++_numBufferSourcesToResolve; // Atomic
    }
}


/*
void
HydraPassthroughResourceRegistry::AddSources(
        HdBufferArrayRangeSharedPtr const &range,
        HdBufferSourceSharedPtrVector &&sources)
{
    if (sources.empty())
    {
        TF_RUNTIME_ERROR("sources list is empty");
        return;
    }

    // range has to be valid
    if (!(range && range->IsValid()))
    {
        TF_RUNTIME_ERROR("range is null or invalid");
        return;
    }

    // Check that each buffer is valid and if not erase it from the list
    // Can not use standard iterators here as erasing invalidates them
    // also the vector is unordered, so we can do a quick erase
    // by moving the item off the end of the vector.
    //
    // I don't know when we expect this list fo have invalid entries, but
    // this is done elsewhere so we will keep the same behavior.
    size_t srcNum = 0;
    while (srcNum < sources.size()) {
        if (sources[srcNum]->IsValid()) {
            if (sources[srcNum]->HasPreChainedBuffer()) {
                AddSource(sources[srcNum]->GetPreChainedBuffer());
            }
            ++srcNum;
        } else {
            TF_RUNTIME_ERROR("Source Buffer for %s is invalid",
                             sources[srcNum]->GetName().GetText());
            
            // Move the last item in the vector over
            // this one.  If it is the last item
            // it will copy over itself and the pop
            // will remove it anyway.
            sources[srcNum] = sources.back();
            sources.pop_back();

            // Don't increment srcNum as it now points
            // to the new item or is off the end of the vector
        }
    }

    // Check for no-valid buffer case
    if (!sources.empty()) {
        _numBufferSourcesToResolve += sources.size();
        _pendingSources.emplace_back(
            range, std::move(sources));

        TF_VERIFY(range.use_count() >=2);
    }
}

void
HydraPassthroughResourceRegistry::AddSource(
        HdBufferArrayRangeSharedPtr const &range,
        HdBufferSourceSharedPtr const &source)
{
    if ((!source) || (!range))
    {
        TF_RUNTIME_ERROR("An input pointer is null");
        return;
    }

    // range has to be valid
    if (!range->IsValid())
    {
        TF_RUNTIME_ERROR("range is invalid");
        return;
    }

    // Buffer has to be valid
    if (!source->IsValid())
    {
        TF_RUNTIME_ERROR("source buffer for %s is invalid",
                          source->GetName().GetText());
        return;
    }

    if (source->HasPreChainedBuffer()) {
        AddSource(source->GetPreChainedBuffer());
    }

    _pendingSources.emplace_back(range, source);
    ++_numBufferSourcesToResolve;  // Atomic
}

void
HydraPassthroughResourceRegistry::AddSource(
        HdBufferSourceSharedPtr const &source)
{
    if (!source)
    {
        TF_RUNTIME_ERROR("source pointer is null");
        return;
    }

    // Buffer has to be valid
    if (!source->IsValid())
    {
        TF_RUNTIME_ERROR("source buffer for %s is invalid",
                         source->GetName().GetText());
        return;
    }

    if (source->HasPreChainedBuffer()) {
        AddSource(source->GetPreChainedBuffer());
    }

    _pendingSources.emplace_back(HdBufferArrayRangeSharedPtr(), source);
    ++_numBufferSourcesToResolve; // Atomic
}
*/

void
HydraPassthroughResourceRegistry::_Commit()
{
    // Staging buffer size uses an atomic in anticipation of the
    // Resolve loop being multithreaded.
    std::atomic_size_t stagingBufferSize { 0 };

    TF_STATUS("Committing %zu pending buffer sources", _pendingSources.size());

    // TODO: requests should be sorted by resource, and range.
    {
        HD_TRACE_SCOPE("Resolve");
        // 1. resolve & resize phase:
        // for each pending source, resolve and check if it needs buffer
        // reallocation or not.

        std::atomic_size_t numBufferSourcesResolved { 0 };
        int numIterations = 0;

        // iterate until all buffer sources have been resolved.
        while (numBufferSourcesResolved < _numBufferSourcesToResolve) {
            // iterate over all pending sources
            WorkParallelForEach(_pendingSources.begin(), _pendingSources.end(),
                [&numBufferSourcesResolved, &stagingBufferSize](
                        _PendingSource &req) {
                    for (HdBufferSourceSharedPtr const& source: req.sources) {
                        // execute computation.
                        // call IsResolved first since Resolve is virtual and
                        // could be costly.
                        printf("Resolving buffer source: %s\n", source->GetName().GetText());
                        if (!source->IsResolved()) {
                            if (source->Resolve()) {
                                TF_VERIFY(source->IsResolved(), 
                                "Name = %s", source->GetName().GetText());

                                ++numBufferSourcesResolved;

                                // Calculate the size of the staging buffer.
                                // XXX no longer storing ranges in the sources
                                /*
                                if (req.range && req.range->RequiresStaging()) {
                                    const size_t numElements =
                                        source->GetNumElements();
                                    // Avoid calling functions on 
                                    // HdNullBufferSources
                                    if (numElements > 0) {
                                        stagingBufferSize += numElements *
                                            HdDataSizeOfTupleType(
                                                source->GetTupleType());
                                    }
                                    stagingBufferSize += 
                                        _GetChainedStagingSize(source);
                                }
                                */
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

        // XXX no more ranges
        /*
        for (_PendingSource &req: _pendingSources) {
            // We resize using the size of the first source in the request
            // since all sources for the request will have the same size.
            if (req.range && TF_VERIFY(!req.sources.empty())) {
                req.range->Resize(req.sources[0]->GetNumElements());
            }
        }
        */

        TF_VERIFY(numBufferSourcesResolved == _numBufferSourcesToResolve);
    }

    {
        HD_TRACE_SCOPE("Copy");
        // 4. copy phase:
        //
        //_stagingBuffer->Resize(stagingBufferSize);

        printf("XX===================================================\n");
        for (_PendingSource &pendingSource : _pendingSources) {

            printf("Pending source for id=%s, type=%d\n", pendingSource.id.GetText(), (int)pendingSource.type);
            
            // XXX Now extract the computed data
            for (HdBufferSourceSharedPtr const& src : pendingSource.sources) {

                _renderData->CopyPrimvarBufferSource(pendingSource.id, src, pendingSource.type, pendingSource.interpolation);

                // Print each resolved buffer to see what we're working with
                printf("Resolved buffer source: %s, numElements: %zu\n",
                    src->GetName().GetText(), src->GetNumElements());
                void const* data = src->GetData();
                HdTupleType tupleType = src->GetTupleType();
                printf("Data pointer: %p, tupleType: (type: %d, count: %zu)\n",
                    data, tupleType.type, tupleType.count);
                printf("interpolation: %d %s\n", pendingSource.interpolation, TfEnum::GetDisplayName(pendingSource.interpolation).c_str());
                if (tupleType.type == HdTypeInvalid) {
                    TF_WARN("Buffer source %s has invalid data type.",
                            src->GetName().GetText());
                }
                else if (tupleType.type == HdType::HdTypeFloatVec2) {
                    for (size_t i = 0; i < src->GetNumElements(); ++i) {
                        const float* vec2Data = static_cast<const float*>(data) + i * 2;
                        printf("(%f, %f) ", vec2Data[0], vec2Data[1]);
                    }
                    printf("\n");
                }
                if (src->HasChainedBuffer()) {
                    HdBufferSourceSharedPtrVector chainedSrcs = src->GetChainedBuffers();
                    for (auto& c : chainedSrcs) {
                        _renderData->CopyPrimvarBufferSource(pendingSource.id, c, pendingSource.type, pendingSource.interpolation);
                        printf("  Chained buffer source: %s, numElements: %zu\n",
                            c->GetName().GetText(), c->GetNumElements());
                        void const* chainedData = c->GetData();
                        HdTupleType chainedTupleType = c->GetTupleType();
                        printf("  Data pointer: %p, tupleType: (type: %d, count: %zu)\n",
                            chainedData, chainedTupleType.type, chainedTupleType.count);
                    }
                }
            }
        }
        printf("XX===================================================\n");
        fflush(stdout);
        // XXX ranges removed
        /*
        for (_PendingSource &pendingSource : _pendingSources) {
            HdBufferArrayRangeSharedPtr &dstRange = pendingSource.range;
            // CPU computation may not have a range. (e.g. adjacency)
            if (!dstRange) continue;

            // CPU computation may result in an empty buffer source
            // (e.g. GPU quadrangulation table could be empty for quad only
            // mesh)
            if (dstRange->GetNumElements() == 0) continue;

            for (auto const& src : pendingSource.sources) {// execute copy
                dstRange->CopyData(src);

                // also copy any chained buffers
                _CopyChainedBuffers(src, dstRange);
            }
        }
        */
    }

    // release sources
    WorkParallelForEach(_pendingSources.begin(), _pendingSources.end(),
                        [](_PendingSource &ps) {
                            //ps.range.reset();
                            ps.sources.clear();
                        });

    _pendingSources.clear();
    _numBufferSourcesToResolve = 0;
}

void
HydraPassthroughResourceRegistry::_GarbageCollect()
{
    
}

PXR_NAMESPACE_CLOSE_SCOPE
