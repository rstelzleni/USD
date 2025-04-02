//
// Copyright 2025 Pixar
//
// Licensed under the terms set forth in the LICENSE.txt file available at
// https://openusd.org/license.

#include "pxr/imaging/hdsi/prefixPathPruningSceneIndex.h"

#include "pxr/imaging/hd/sceneIndexPrimView.h"
#include "pxr/base/trace/trace.h"
#include "pxr/usd/sdf/path.h"

#include <algorithm>

PXR_NAMESPACE_OPEN_SCOPE

TF_DEFINE_PUBLIC_TOKENS(HdsiPrefixPathPruningSceneIndexTokens,
                        HDSI_PREFIX_PATH_PRUNING_SCENE_INDEX_TOKENS);

namespace
{

SdfPathVector
_GetExcludePathPrefixes(const HdContainerDataSourceHandle &container)
{
    if (!container) {
        return {};
    }

    using DataSource = HdTypedSampledDataSource<SdfPathVector>;
    auto const ds =
        DataSource::Cast(
            container->Get(
                HdsiPrefixPathPruningSceneIndexTokens->excludePathPrefixes));
    if (!ds) {
        return {};
    }

    return ds->GetTypedValue(0.0f);
}

/// Queries \p container to get the exclude path prefixes and returns a sorted
/// vector of exclude paths with any duplicates or descendent
/// paths removed.
///
SdfPathVector
_GetSanitizedExcludePaths(const HdContainerDataSourceHandle &container)
{
    SdfPathVector paths = _GetExcludePathPrefixes(container);
    SdfPath::RemoveDescendentPaths(&paths);
    return paths;
}

bool
_IsPrunedImpl(
    const SdfPath &primPath, const SdfPathVector &sortedExcludePaths)
{
    // Since the exclude paths are sorted and stripped of descendents,
    // it suffices to check the lower bound for equality and optionally just
    // the previous element for a prefix match.
    // The previous element can be:
    // (a) a sibling or a sibling descendent prim
    // (b) an ancestor prim
    // (c) a prim from a disjoint subtree
    //
    // Only (b) prunes the primPath.
    //
    auto it = std::lower_bound(
        sortedExcludePaths.begin(), sortedExcludePaths.end(), primPath);
    
    if (it != sortedExcludePaths.end() && *it == primPath) {
        return true;
    }
    
    if (it != sortedExcludePaths.begin()) {
        --it;
        if (primPath.HasPrefix(*it)) {
            return true;
        }
    }
    
    return false;
}

/// Returns prefix paths in \p a that are not covered by prefix paths in \p b.
/// i.e. elements in \p a that are not prefixed by any element in \p b.
///
SdfPathVector
_ComputeUncoveredPrefixes(
    const SdfPathVector &a, const SdfPathVector &b)
{
    SdfPathVector result;
    for (const SdfPath &path : a) {
        if (std::none_of(
                b.begin(), b.end(),
                [&path](const SdfPath &prefix) {
                    return path.HasPrefix(prefix);
                })) {
            result.push_back(path);
        }
    }

    return result;
}

}

/* static */
HdsiPrefixPathPruningSceneIndexRefPtr
HdsiPrefixPathPruningSceneIndex::New(
    HdSceneIndexBaseRefPtr const &inputSceneIndex,
    HdContainerDataSourceHandle const &inputArgs)
{
    return TfCreateRefPtr(
        new HdsiPrefixPathPruningSceneIndex(
            inputSceneIndex, inputArgs));
}

HdSceneIndexPrim
HdsiPrefixPathPruningSceneIndex::GetPrim(const SdfPath &primPath) const
{
    if (_sortedExcludePaths.empty() || !_IsPruned(primPath)) {
        return _GetInputSceneIndex()->GetPrim(primPath);
    }

    static const HdSceneIndexPrim emptyPrim;
    return emptyPrim;
}

SdfPathVector
HdsiPrefixPathPruningSceneIndex::GetChildPrimPaths(
    const SdfPath &primPath) const
{
    TRACE_FUNCTION();

    const bool haveExcludePaths = !_sortedExcludePaths.empty();
    if (haveExcludePaths && _IsPruned(primPath)) {
        return {};
    }

    SdfPathVector childPaths =
        _GetInputSceneIndex()->GetChildPrimPaths(primPath);

    if (haveExcludePaths) {
        _RemovePrunedChildren(&childPaths);
    }

    return childPaths;
}

void
HdsiPrefixPathPruningSceneIndex::SetExcludePathPrefixes(
    SdfPathVector paths)
{
    TRACE_FUNCTION();

    SdfPathVector newPrefixes(std::move(paths));
    SdfPath::RemoveDescendentPaths(&newPrefixes);
    
    if (newPrefixes == _sortedExcludePaths) {
        return;
    }

    const SdfPathVector oldPrefixes(std::move(_sortedExcludePaths));

    if (!_IsObserved()) {
        _sortedExcludePaths = std::move(newPrefixes);
        return;
    }

    // From the new and old prefixes, we want to determine:
    // (a) the prefixes that are no longer pruned
    // (b) the prefixes that are newly pruned
    //
    SdfPathVector noLongerPrunedPrefixes =
        _ComputeUncoveredPrefixes(oldPrefixes, newPrefixes);

    HdSceneIndexBaseRefPtr const &inputSi = _GetInputSceneIndex();

    // Add all the prims in each prefix's subtree. Note that this may include
    // descendent prims that are pruned by the new prefixes. We send the added
    // notices first and then the removed notices to address this.
    // 
    HdSceneIndexObserver::AddedPrimEntries addedEntries;
    for (const SdfPath &prefix : noLongerPrunedPrefixes) {

        for (const SdfPath &primPath : HdSceneIndexPrimView(inputSi, prefix)) {
            
            addedEntries.emplace_back(
                primPath, inputSi->GetPrim(primPath).primType);
        }
    }

    // Use set difference to remove prefixes that were already pruned (i.e.
    // duplicates). These paths are guaranteed to not be in
    // noLongerPrunedPrefixes.
    //
    SdfPathVector newlyPrunedPrefixes;
    std::set_difference(
        newPrefixes.begin(), newPrefixes.end(),
        oldPrefixes.begin(), oldPrefixes.end(),
        std::back_inserter(newlyPrunedPrefixes));
    
    HdSceneIndexObserver::RemovedPrimEntries removedEntries;
    for (const SdfPath &prefix : newlyPrunedPrefixes) {
        removedEntries.emplace_back(prefix);
    }

    _sortedExcludePaths = std::move(newPrefixes);
    _SendPrimsAdded(addedEntries);
    _SendPrimsRemoved(removedEntries);
}

/// Returns \p inputEntries if no notice entries are pruned. Otherwise, copies
/// the entries that are not pruned into \p filteredEntries and returns it.
///
/// This helps avoid an unnecessary copy of the input entries when no pruning
/// is needed.
///
template <typename NoticeEntries>
static const NoticeEntries&
_RemovePrunedNoticeEntries(
    const SdfPathVector &sortedExcludePaths,
    const NoticeEntries &inputEntries,
    NoticeEntries *filteredEntries)
{
    std::copy_if(
        inputEntries.begin(), inputEntries.end(),
        std::back_inserter(*filteredEntries),
        [&sortedExcludePaths](const auto &entry) {
            return !_IsPrunedImpl(entry.primPath, sortedExcludePaths);
        });
    
    if (filteredEntries->empty()) {
        return inputEntries;
    }
    return *filteredEntries;
}

void
HdsiPrefixPathPruningSceneIndex::_PrimsAdded(
    const HdSceneIndexBase &sender,
    const HdSceneIndexObserver::AddedPrimEntries &entries)
{
    if (!_IsObserved()) {
        return;
    }

    if (_sortedExcludePaths.empty()) {
        _SendPrimsAdded(entries);
        return;
    }

    HdSceneIndexObserver::AddedPrimEntries filteredEntries;
    _SendPrimsAdded(
        _RemovePrunedNoticeEntries(
            _sortedExcludePaths, entries, &filteredEntries));
}

void
HdsiPrefixPathPruningSceneIndex::_PrimsRemoved(
    const HdSceneIndexBase &sender,
    const HdSceneIndexObserver::RemovedPrimEntries &entries)
{
    if (!_IsObserved()) {
        return;
    }

    if (_sortedExcludePaths.empty()) {
        _SendPrimsRemoved(entries);
        return;
    }

    HdSceneIndexObserver::RemovedPrimEntries filteredEntries;
    _SendPrimsRemoved(
        _RemovePrunedNoticeEntries(
            _sortedExcludePaths, entries, &filteredEntries));
}

void
HdsiPrefixPathPruningSceneIndex::_PrimsDirtied(
    const HdSceneIndexBase &sender,
    const HdSceneIndexObserver::DirtiedPrimEntries &entries)
{
    if (!_IsObserved()) {
        return;
    }

    if (_sortedExcludePaths.empty()) {
        _SendPrimsDirtied(entries);
        return;
    }

    HdSceneIndexObserver::DirtiedPrimEntries filteredEntries;
    _SendPrimsDirtied(
        _RemovePrunedNoticeEntries(
            _sortedExcludePaths, entries, &filteredEntries));
}

HdsiPrefixPathPruningSceneIndex::HdsiPrefixPathPruningSceneIndex(
    HdSceneIndexBaseRefPtr const &inputSceneIndex,
    HdContainerDataSourceHandle const &inputArgs)
    : HdSingleInputFilteringSceneIndexBase(inputSceneIndex)
    , _sortedExcludePaths(_GetSanitizedExcludePaths(inputArgs))
{
    // There cannot be an observer when we're creating a filtering scene index.
    // So, we don't need to traverse the input scene to remove pruned prim
    // subtrees.
}

HdsiPrefixPathPruningSceneIndex::
~HdsiPrefixPathPruningSceneIndex() = default;

bool
HdsiPrefixPathPruningSceneIndex::_IsPruned(const SdfPath &primPath) const
{
    return _IsPrunedImpl(primPath, _sortedExcludePaths);
}

void
HdsiPrefixPathPruningSceneIndex::_RemovePrunedChildren(
    SdfPathVector *childPaths) const
{
    TRACE_FUNCTION();

    if (!childPaths) {
        return;
    }
    if (childPaths->empty()) {
        return;
    }
    
    childPaths->erase(
        std::remove_if(
            childPaths->begin(), childPaths->end(),
            [this](const SdfPath &childPath) {
                return _IsPruned(childPath);
            }),
        childPaths->end());
}

PXR_NAMESPACE_CLOSE_SCOPE
