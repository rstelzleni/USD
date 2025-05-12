//
// Copyright 2025 Pixar
//
// Licensed under the terms set forth in the LICENSE.txt file available at
// https://openusd.org/license.
//
#include "pxr/imaging/hd/cachingSceneIndex.h"

#include "pxr/base/trace/trace.h"
#include "pxr/base/work/utils.h"

PXR_NAMESPACE_OPEN_SCOPE

HdCachingSceneIndex::HdCachingSceneIndex(
        HdSceneIndexBaseRefPtr const &inputScene)
  : HdSingleInputFilteringSceneIndexBase(inputScene)
{
}

HdCachingSceneIndex::~HdCachingSceneIndex() = default;

HdSceneIndexPrim
HdCachingSceneIndex::GetPrim(const SdfPath &primPath) const
{
    TRACE_FUNCTION();
    
    // Check the hierarchy cache
    const _PrimTable::const_iterator i = _prims.find(primPath);
    // SdfPathTable will default-construct entries for ancestors
    // as needed to represent hierarchy, so double-check the
    // dataSource to confirm presence os a cached prim
    if (i != _prims.end() && i->second) {
        return *i->second;
    }

    // Check the recent prims cache
    {
        // Use a scope to minimize lifetime of tbb accessor
        // for maximum concurrency
        _RecentPrimTable::const_accessor accessor;
        if (_recentPrims.find(accessor, primPath)) {
            return accessor->second;
        }
    }

    // No cache entry found; query input scene
    HdSceneIndexPrim prim = _GetInputSceneIndex()->GetPrim(primPath);

    // Store in the recent prims cache
    if (!_recentPrims.insert(std::make_pair(primPath, prim))) {
        // Another thread inserted this entry.  Since dataSources
        // are stateful, return that one.
        _RecentPrimTable::accessor accessor;
        if (TF_VERIFY(_recentPrims.find(accessor, primPath))) {
            prim = accessor->second;
        }
    }
    return prim;
}

SdfPathVector
HdCachingSceneIndex::GetChildPrimPaths(const SdfPath &primPath) const
{
    // we don't change topology so we can dispatch to input
    return _GetInputSceneIndex()->GetChildPrimPaths(primPath);
}

void
HdCachingSceneIndex::_PrimsAdded(
        const HdSceneIndexBase &sender,
        const HdSceneIndexObserver::AddedPrimEntries &entries)
{
    TRACE_FUNCTION();

    _ConsolidateRecentPrims();

    for (const HdSceneIndexObserver::AddedPrimEntry &entry : entries) {
        const _PrimTable::iterator i = _prims.find(entry.primPath);
        if (i != _prims.end() && i->second) {
            WorkSwapDestroyAsync(i->second->dataSource);
            i->second = std::nullopt;
        }
    }
    
    _SendPrimsAdded(entries);
}

void
HdCachingSceneIndex::_PrimsRemoved(
    const HdSceneIndexBase &sender,
    const HdSceneIndexObserver::RemovedPrimEntries &entries)
{
    TRACE_FUNCTION();

    _ConsolidateRecentPrims();

    for (const HdSceneIndexObserver::RemovedPrimEntry &entry : entries) {
        if (entry.primPath.IsAbsoluteRootPath()) {
            // Special case removing the whole scene, since this is a common
            // shutdown operation.
            _prims.ClearInParallel();
            TfReset(_prims);
        } else {
            const auto startEndIt = _prims.FindSubtreeRange(entry.primPath);
            for (auto it = startEndIt.first; it != startEndIt.second; ++it) {
                if (it->second) {
                    WorkSwapDestroyAsync(it->second->dataSource);
                }
            }
            if (startEndIt.first != startEndIt.second) {
                _prims.erase(startEndIt.first);
            }
        }
    }
    _SendPrimsRemoved(entries);
}


void
HdCachingSceneIndex::_PrimsDirtied(
    const HdSceneIndexBase &sender,
    const HdSceneIndexObserver::DirtiedPrimEntries &entries)
{
    TRACE_FUNCTION();

    _ConsolidateRecentPrims();

    for (const HdSceneIndexObserver::DirtiedPrimEntry &entry : entries) {
        if (entry.dirtyLocators.Contains(HdDataSourceLocator::EmptyLocator())) {
            const _PrimTable::iterator i = _prims.find(entry.primPath);
            if (i != _prims.end() && i->second) {
                WorkSwapDestroyAsync(i->second->dataSource);
                i->second = std::nullopt;
            }
        }
    }

    _SendPrimsDirtied(entries);
}

void
HdCachingSceneIndex::_ConsolidateRecentPrims()
{
    TRACE_FUNCTION();

    for (auto &entry: _recentPrims) {
        _prims[entry.first] = std::move(entry.second);
    }
    _recentPrims.clear();
}

PXR_NAMESPACE_CLOSE_SCOPE
