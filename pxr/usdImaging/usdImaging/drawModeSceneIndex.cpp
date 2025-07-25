//
// Copyright 2022 Pixar
//
// Licensed under the terms set forth in the LICENSE.txt file available at
// https://openusd.org/license.

#include "pxr/usdImaging/usdImaging/drawModeSceneIndex.h"

#include "pxr/usdImaging/usdImaging/drawModeStandin.h"
#include "pxr/usdImaging/usdImaging/geomModelSchema.h"

#include "pxr/usd/sdf/path.h"

#include "pxr/base/trace/trace.h"
#include "pxr/base/work/loops.h"

#include "tbb/concurrent_vector.h"

PXR_NAMESPACE_OPEN_SCOPE

namespace
{

// Resolve draw mode for prim from input scene index.
// Default draw mode can be expressed by either the empty token
// or UsdGeomTokens->default_.
TfToken
_GetDrawMode(const HdSceneIndexPrim &prim)
{
    static const TfToken empty;

    UsdImagingGeomModelSchema geomModelSchema =
        UsdImagingGeomModelSchema::GetFromParent(prim.dataSource);

    HdBoolDataSourceHandle const applySrc = geomModelSchema.GetApplyDrawMode();
    if (!applySrc) {
        return empty;
    }
    if (!applySrc->GetTypedValue(0.0f)) {
        return empty;
    }
    
    HdTokenDataSourceHandle const modeSrc = geomModelSchema.GetDrawMode();
    if (!modeSrc) {
        return empty;
    }
    return modeSrc->GetTypedValue(0.0f);
}

}

UsdImagingDrawModeSceneIndexRefPtr
UsdImagingDrawModeSceneIndex::New(
    const HdSceneIndexBaseRefPtr &inputSceneIndex,
    const HdContainerDataSourceHandle &inputArgs)
{
    return TfCreateRefPtr(
        new UsdImagingDrawModeSceneIndex(
            inputSceneIndex, inputArgs));
}

UsdImagingDrawModeSceneIndex::UsdImagingDrawModeSceneIndex(
    const HdSceneIndexBaseRefPtr &inputSceneIndex,
    const HdContainerDataSourceHandle &inputArgs)
  : HdSingleInputFilteringSceneIndexBase(inputSceneIndex)
{
    TRACE_FUNCTION();

    const SdfPath &rootPath = SdfPath::AbsoluteRootPath();

    const HdSceneIndexPrim prim = _GetInputSceneIndex()->GetPrim(rootPath);

    _RecursePrims(_GetDrawMode(prim), rootPath, prim, nullptr);
}

UsdImagingDrawModeSceneIndex::~UsdImagingDrawModeSceneIndex() = default;

static
auto _FindPrefixOfPath(
    const std::map<SdfPath, UsdImaging_DrawModeStandinSharedPtr> &container,
    const SdfPath &path)
{
    // Use std::map::lower_bound over std::lower_bound since the latter
    // is slow given that std::map iterators are not random access.
    auto it = container.lower_bound(path);
    if (it != container.end() && path == it->first) {
        // Path itself is in the container
        return it;
    }
    // If a prefix of path is in container, it will point to the next element
    // in the container, rather than the prefix itself.
    if (it == container.begin()) {
        return container.end();
    }
    --it;
    if (path.HasPrefix(it->first)) {
        return it;
    }
    return container.end();
}

UsdImaging_DrawModeStandinSharedPtr
UsdImagingDrawModeSceneIndex::_FindStandinForPrimOrAncestor(
    const SdfPath &path,
    bool * const isPathDescendant) const
{
    const auto it = _FindPrefixOfPath(_prims, path);
    if (it == _prims.end()) {
        return nullptr;
    }
    if (isPathDescendant) {
        *isPathDescendant =
            path.GetPathElementCount() > it->first.GetPathElementCount();
    }
    return it->second;
}

HdSceneIndexPrim
UsdImagingDrawModeSceneIndex::GetPrim(
    const SdfPath &primPath) const
{
    TRACE_FUNCTION();

    // Do we have this prim path or an ancestor prim path in the
    // _prims map?
    if (UsdImaging_DrawModeStandinSharedPtr const standin =
            _FindStandinForPrimOrAncestor(primPath)) {
        return standin->GetPrim(primPath);
    }

    return _GetInputSceneIndex()->GetPrim(primPath);
}

static
bool
_IsImmediateChildOf(const SdfPath &path, const SdfPath &parentPath)
{
    return
        path.GetPathElementCount() - parentPath.GetPathElementCount() == 1 &&
        path.HasPrefix(parentPath);
}

SdfPathVector
UsdImagingDrawModeSceneIndex::GetChildPrimPaths(
    const SdfPath &primPath) const
{
    TRACE_FUNCTION();

    // Do we have this prim path or an ancestor prim path in the
    // _prims map?
    if (UsdImaging_DrawModeStandinSharedPtr const standin =
            _FindStandinForPrimOrAncestor(primPath)) {
        // standin->GetDescendantPrimPaths() gives all descendants, but
        // we just want the queried prim's direct children so we only
        // want the descendant paths with the full queried path as prefix and
        // exactly one additional path component. This works whether the
        // queried path is for the typeless container (children: standin prim +
        // materials), the standin prim (children: subsets), a subset (children:
        // none), or a material (children: none).
        SdfPathVector paths;
        for (const SdfPath &path : standin->GetPrimPaths()) {
            if (_IsImmediateChildOf(path, primPath)) {
                paths.push_back(path);
            }
        }
        return paths;
    }

    return _GetInputSceneIndex()->GetChildPrimPaths(primPath);
}

void
UsdImagingDrawModeSceneIndex::_DeleteSubtree(const SdfPath &path)
{
    auto it = _prims.lower_bound(path);
    while (it != _prims.end() && it->first.HasPrefix(path)) {
        it = _prims.erase(it);
    }
}

// Called from _PrimsDirtied on main-thread so we have enough stack space
// to just recurse.
void
UsdImagingDrawModeSceneIndex::_RecursePrims(
    const TfToken &mode,
    const SdfPath &path,
    const HdSceneIndexPrim &prim,
    HdSceneIndexObserver::AddedPrimEntries *entries)
{
    if (UsdImaging_DrawModeStandinSharedPtr standin =
            UsdImaging_GetDrawModeStandin(mode, path, prim.dataSource)) {
        // The prim needs to be replaced by stand-in geometry.
        // Send added entries for stand-in geometry.
        if (entries) {
            standin->ComputePrimAddedEntries(entries);
        }
        // And store it.
        _prims[path] = std::move(standin);
    } else {
        // Mark prim as added and recurse to children.
        if (entries) {
            entries->push_back({path, prim.primType});
        }
        const HdSceneIndexBaseRefPtr &s = _GetInputSceneIndex();
        for (const SdfPath &childPath : s->GetChildPrimPaths(path)) {
            const HdSceneIndexPrim prim = s->GetPrim(childPath);
            _RecursePrims(_GetDrawMode(prim), childPath, prim, entries);
        }
    }
}

void
UsdImagingDrawModeSceneIndex::_PrimsAdded(
    const HdSceneIndexBase &sender,
    const HdSceneIndexObserver::AddedPrimEntries &entries)
{
    TRACE_FUNCTION();

    HdSceneIndexObserver::AddedPrimEntries newEntries;
    HdSceneIndexObserver::RemovedPrimEntries removedEntries;

    // Loop over notices to determine the prims that have a draw mode. Since
    // the prim container is used to determine this, it can be quite expensive.
    // So, we parallelize the work below with the caveat that we may be querying
    // descendant prims under a tracked prim that has a draw mode.
    // XXX We preserve the order of notice entries to workaround a bug in 
    // backend emulation in the handling of geom subset prims.
    //
    using _AddedEntryAndPrimPair =
        std::pair<HdSceneIndexObserver::AddedPrimEntry, HdSceneIndexPrim>;
    tbb::concurrent_vector<_AddedEntryAndPrimPair> entryPrimPairs(
        entries.size());

    {
        TRACE_FUNCTION_SCOPE("Notice processing - prim query");
        WorkParallelForN(entries.size(),
            [&](size_t begin, size_t end) {
                for (size_t i = begin; i < end; ++i) {
                    const HdSceneIndexObserver::AddedPrimEntry &entry =
                        entries[i];
                    
                    const SdfPath &path = entry.primPath;
                    const HdSceneIndexPrim prim =
                        _GetInputSceneIndex()->GetPrim(path);
                    entryPrimPairs[i] = {entry, prim};
                }
            });
    }

    // Serial loop for simplicity because _prims is not thread safe.
    //
    for (const auto& [entry, prim] : entryPrimPairs) {
        const SdfPath &path = entry.primPath;

        // Suppress prims from input scene delegate that have an ancestor
        // with a draw mode.
        bool isPathDescendant = false;
        if (UsdImaging_DrawModeStandinSharedPtr standin =
               _FindStandinForPrimOrAncestor(path, &isPathDescendant)) {
            if (isPathDescendant) {
                continue;
            }
        }

        const TfToken drawMode = _GetDrawMode(prim);

        if (UsdImaging_DrawModeStandinSharedPtr standin =
            UsdImaging_GetDrawModeStandin(
                drawMode, path, prim.dataSource)) {

            // Sending out removed entry here for the following scenario:
            // Assume that the input to the draw mode scene index has a
            // prim with non-default draw mode at /Foo and a prim at /Foo/Bar.
            // The draw mode scene index has not yet received a prims added
            // call for /Foo (thus, there is no entry for /Foo in
            // UsdImagingDrawModeSceneIndex::_prim), yet a client scene
            // index asked for the prim at /Foo/Bar.
            // At this point, the draw mode scene index returns a valid
            // prim for GetPrim(/Foo/Bar) with prim type determined from the
            // input scene index. This is incorrect as the prim should be
            // dropped because of /Foo's draw mode. Similarly, for
            // GetChildPrimPaths.
            // When the PrimsAdded message for /Foo arrived, the
            // UsdImagingDrawModeSceneIndex will update _prim. And it can
            // now rectify the situation by sending out a removes prim
            // message for /Foo.
            //
            // Note that this happens when there are prototype propagating
            // scene indices have been connected to a
            // UsdImagingStageSceneIndex before the call to
            // UsdImagingStageSceneIndex::SetStage.
            // The prototype propagating scene index inserts propagated
            // prototypes into the merging scene index. When a scene
            // index is added to the merging scene index, it traverses
            // it through GetChildPrimPaths to emit the necessary prims
            // added messages. In particular, it might call
            // GetChildPrimPaths for a prim inside a prototype before
            // the PrimsAdded message for that prim was emitted by the
            // UsdImagingStageSceneIndex.
            //
            _DeleteSubtree(path);
            removedEntries.push_back({path});

            // The prim needs to be replaced by stand-in geometry.
            standin->ComputePrimAddedEntries(&newEntries);
            _prims[path] = std::move(standin);
        } else {
            // Just forward added entry.
            newEntries.push_back(entry);
        }
    }

    _SendPrimsRemoved(removedEntries);
    _SendPrimsAdded(newEntries);
}

void
UsdImagingDrawModeSceneIndex::_PrimsRemoved(
    const HdSceneIndexBase &sender,
    const HdSceneIndexObserver::RemovedPrimEntries &entries)
{
    TRACE_FUNCTION();

    if (!_prims.empty()) {
        for (const HdSceneIndexObserver::RemovedPrimEntry &entry : entries) {
            // Delete corresponding stand-in geometry.
            _DeleteSubtree(entry.primPath);
        }
    }

    if (!_IsObserved()) {
        return;
    }

    _SendPrimsRemoved(entries);
}

void
UsdImagingDrawModeSceneIndex::_PrimsDirtied(
    const HdSceneIndexBase &sender,
    const HdSceneIndexObserver::DirtiedPrimEntries &entries)
{
    TRACE_FUNCTION();
    
    // Determine the paths of all prims whose draw mode might have changed.
    std::set<SdfPath> paths;

    static const HdDataSourceLocatorSet drawModeLocators{
        UsdImagingGeomModelSchema::GetDefaultLocator().Append(
            UsdImagingGeomModelSchemaTokens->drawMode),
        UsdImagingGeomModelSchema::GetDefaultLocator().Append(
            UsdImagingGeomModelSchemaTokens->applyDrawMode)};
            
    for (const HdSceneIndexObserver::DirtiedPrimEntry &entry : entries) {
        if (drawModeLocators.Intersects(entry.dirtyLocators)) {
            paths.insert(entry.primPath);
        }
    }

    HdSceneIndexObserver::RemovedPrimEntries removedEntries;
    HdSceneIndexObserver::AddedPrimEntries addedEntries;

    if (!paths.empty()) {
        // Draw mode changed means we need to remove the stand-in geometry
        // or prims forwarded from the input scene delegate and then (re-)add
        // the stand-in geometry or prims from the input scene delegate.
        
        // Set this to skip all descendants of a given path.
        SdfPath lastPath;
        for (const SdfPath &path : paths) {
            // Skip all descendants of lastPath - if lastPath is not empty.
            if (path.HasPrefix(lastPath)) {
                continue;
            }
            lastPath = SdfPath();

            // Suppress prims from input scene delegate that have an ancestor
            // with a draw mode.
            bool isPathDescendant = false;
            if (UsdImaging_DrawModeStandinSharedPtr standin =
                _FindStandinForPrimOrAncestor(path, &isPathDescendant)) {
                if (isPathDescendant) {
                    continue;
                }
            }
            
            // Determine new draw mode.
            const HdSceneIndexPrim prim = _GetInputSceneIndex()->GetPrim(path);
            const TfToken drawMode = _GetDrawMode(prim);

            const auto it = _prims.find(path);
            if (it == _prims.end()) {
                // Prim used to have default draw mode.
                if (UsdImaging_DrawModeStandinSharedPtr standin =
                        UsdImaging_GetDrawModeStandin(
                            drawMode, path, prim.dataSource)) {
                    // Prim now has non-default draw mode and we need to use
                    // stand-in geometry.
                    //
                    // Delete old geometry.
                    _DeleteSubtree(path);
                    removedEntries.push_back({path});
                    // Add new stand-in geometry.
                    standin->ComputePrimAddedEntries(&addedEntries);
                    _prims[path] = std::move(standin);
                    // Do not traverse descendants of this prim.
                    lastPath = path;
                }
            } else {
                if (it->second->GetDrawMode() != drawMode) {
                    // Draw mode has changed (including changed to default).
                    //
                    // Delete old geometry.
                    _DeleteSubtree(path);
                    removedEntries.push_back({path});
                    // Different scenarios are possible:
                    // 1. The prim was switched to default draw mode. We need
                    //    to recursively pull the geometry from the input scene
                    //    index again and send corresponding added entries.
                    //    If the prim has a descendant with non-default draw
                    //    mode, the recursion stops and we use stand-in 
                    //    geometry instead.
                    // 2. The prim switched to a different non-default draw
                    //    mode. This can be regarded as the special case where
                    //    the recursion immediately stops.
                    _RecursePrims(drawMode, path, prim, &addedEntries);
                    // Since we recursed to all descendants of the prim, ignore
                    // any descendants here.
                    lastPath = path;
                }
            }
        }
    }

    if (_prims.empty()) {
        if (!removedEntries.empty()) {
            _SendPrimsRemoved(removedEntries);
        }
        if (!addedEntries.empty()) {
            _SendPrimsAdded(addedEntries);
        }
        _SendPrimsDirtied(entries);
        return;
    }

    // Now account for dirtyLocators not related to resolving the draw mode.

    HdSceneIndexObserver::DirtiedPrimEntries dirtiedEntries;
    
    for (const HdSceneIndexObserver::DirtiedPrimEntry &entry : entries) {
        const SdfPath &path = entry.primPath;
        bool isPathDescendant = false;
        UsdImaging_DrawModeStandinSharedPtr const standin =
            _FindStandinForPrimOrAncestor(path, &isPathDescendant);
        if (!standin) {
            // Prim and all its ancestors have default draw mode,
            // just forward entry.
            dirtiedEntries.push_back(entry);
            continue;
        }

        if (isPathDescendant) {
            // Descendants of prims with non-default draw mode can be ignored.
            continue;
        }
        
        // Prim replaced by stand-in geometry has changed. Determine how
        // stand-in geometry is affected by changed attributed on prim.
        // ProcessDirtyLocators will do this; if the prim has changed in a
        // way that requires us to regenerate it (e.g., an axis has been added
        // or removed), it will set needsRefresh to true and we can then call
        // _RefreshDrawModeStandin. Note that _RefreshDrawModeStandin calls
        // _SendPrimsRemoved and _SendPrimsAdded as needed.
        
        bool needsRefresh = false;
        standin->ProcessDirtyLocators(
            entry.dirtyLocators, &dirtiedEntries, &needsRefresh);
        if (needsRefresh) {
            UsdImaging_DrawModeStandinSharedPtr newStandin =
                UsdImaging_GetDrawModeStandin(
                    standin->GetDrawMode(),
                    path,
                    _GetInputSceneIndex()->GetPrim(path).dataSource);
            if (!TF_VERIFY(newStandin)) {
                continue;
            }
            removedEntries.push_back({path});
            newStandin->ComputePrimAddedEntries(&addedEntries);
            _prims[path] = std::move(newStandin);
        }
    }
    if (!removedEntries.empty()) {
        _SendPrimsRemoved(removedEntries);
    }
    if (!addedEntries.empty()) {
        _SendPrimsAdded(addedEntries);
    }
    if (!dirtiedEntries.empty()) {
        _SendPrimsDirtied(dirtiedEntries);
    }
}

PXR_NAMESPACE_CLOSE_SCOPE
