//
// Copyright 2025 Pixar
//
// Licensed under the terms set forth in the LICENSE.txt file available at
// https://openusd.org/license.

#include "pxr/imaging/hdsi/unboundMaterialOverridingSceneIndex.h"

#include "pxr/imaging/hd/materialBindingSchema.h"
#include "pxr/imaging/hd/materialBindingsSchema.h"
#include "pxr/imaging/hd/materialSchema.h"
#include "pxr/imaging/hd/sceneIndexPrimView.h"
#include "pxr/imaging/hd/tokens.h"

#include "pxr/base/work/loops.h"
#include "pxr/base/trace/trace.h"

#include "tbb/concurrent_vector.h"

#include <algorithm>

PXR_NAMESPACE_OPEN_SCOPE

TF_DEFINE_PUBLIC_TOKENS(
    HdsiUnboundMaterialOverridingSceneIndexTokens,
    HDSI_UNBOUND_MATERIAL_PRUNING_SCENE_INDEX_TOKENS);

namespace
{
VtArray<TfToken>
_GetMaterialBindingPurposes(
    HdContainerDataSourceHandle const &inputArgs)
{
    const HdDataSourceBaseHandle ds =
        inputArgs->Get(
            HdsiUnboundMaterialOverridingSceneIndexTokens->
                materialBindingPurposes);

    if (HdTokenArrayDataSourceHandle tokensDs =
            HdTokenArrayDataSource::Cast(ds)) {
        
        return tokensDs->GetTypedValue(0.0f);
    }

    return {};
}

bool
_Contains(
    const std::unordered_set<SdfPath, SdfPath::Hash> &paths,
    const SdfPath &primPath)
{
    return paths.find(primPath) != paths.end();
}

SdfPathVector
_GetBoundMaterialPaths(
    const HdContainerDataSourceHandle &primContainer,
    const VtArray<TfToken> &bindingPurposes)
{
    HdMaterialBindingsSchema bindingsSchema =
        HdMaterialBindingsSchema::GetFromParent(primContainer);
    
    if (!bindingsSchema) {
        return {};
    }

    SdfPathVector materialBindingPaths;

    for (const TfToken &purpose : bindingPurposes) {
        HdMaterialBindingSchema bindingSchema =
            bindingsSchema.GetMaterialBinding(purpose);
        
        const HdPathDataSourceHandle ds = bindingSchema.GetPath();
        if (!ds) {
            continue;
        }

        materialBindingPaths.push_back(ds->GetTypedValue(0.0f));
    }

    return materialBindingPaths;
}

HdDataSourceLocatorSet _ComputeBindingLocators(
    const VtArray<TfToken> &bindingPurposes)
{
    HdDataSourceLocatorSet locators;
    for (const TfToken &purpose : bindingPurposes) {
        locators.insert(
            HdMaterialBindingsSchema::GetDefaultLocator()
                .Append(purpose));
    }
    return locators;
}

} // anon

// static
HdsiUnboundMaterialOverridingSceneIndexRefPtr
HdsiUnboundMaterialOverridingSceneIndex::New(
    HdSceneIndexBaseRefPtr const &inputSceneIndex,
    HdContainerDataSourceHandle const &inputArgs)
{
    return TfCreateRefPtr(new HdsiUnboundMaterialOverridingSceneIndex(
        inputSceneIndex, inputArgs));
}

HdSceneIndexPrim
HdsiUnboundMaterialOverridingSceneIndex::GetPrim(
    const SdfPath &primPath) const
{
    HdSceneIndexPrim prim = _GetInputSceneIndex()->GetPrim(primPath);

    if (prim.primType == HdPrimTypeTokens->material &&
        !_IsBoundMaterial(primPath)) {
        // Clear just the prim container. Note that we don't clear the prim type
        // because this simplifies the processing necessary in the notice 
        // handlers.
        prim.dataSource = nullptr;
    }

    return prim;
}

SdfPathVector
HdsiUnboundMaterialOverridingSceneIndex::GetChildPrimPaths(
    const SdfPath &primPath) const
{
    // This scene index does not remove unbound material prims from the scene
    // topology. It only overrides their prim container.
    return _GetInputSceneIndex()->GetChildPrimPaths(primPath);
}

void
HdsiUnboundMaterialOverridingSceneIndex::_PrimsAdded(
    const HdSceneIndexBase &sender,
    const HdSceneIndexObserver::AddedPrimEntries &entries)
{    
    TRACE_FUNCTION();

    using _ConcurrentPathVector = tbb::concurrent_vector<SdfPath>;
    _ConcurrentPathVector addedMaterialPaths;
    _ConcurrentPathVector boundMaterialPaths;
    // Querying each prim to get the material bindings can be expensive, so we
    // parallelize the processing of the entries.
    {
        TRACE_FUNCTION_SCOPE("Parallel notice processing");
        WorkParallelForN(
            entries.size(),
            [&](size_t begin, size_t end) {
                for (size_t i = begin; i < end; ++i) {
                    const auto &entry = entries[i];
                    
                    if (entry.primType.IsEmpty()) {
                        // Ignore bindings on intermediate prims (like scopes 
                        // and xforms) for whom material bindings are not 
                        // relevant but present from flattening.
                        continue;
                    }
                    if (entry.primType == HdPrimTypeTokens->material) {
                        addedMaterialPaths.push_back(entry.primPath);
                        continue;
                    }
    
                    const HdSceneIndexPrim prim =
                        _GetInputSceneIndex()->GetPrim(entry.primPath);
    
                    const SdfPathVector materialPaths =
                        _GetBoundMaterialPaths(
                            prim.dataSource, _bindingPurposes);
    
                    // concurrent_vector is not thread-safe for range insertion.
                    for (const SdfPath &materialPath : materialPaths) {
                        boundMaterialPaths.push_back(materialPath);
                    }
                }
            });
    }
    
    if (boundMaterialPaths.empty() && addedMaterialPaths.empty()) {
        // No materials or prims with bindings were added.
        _SendPrimsAdded(entries);
        return;
    }

    HdSceneIndexObserver::DirtiedPrimEntries newlyBoundEntries;
    
    std::unordered_set<SdfPath, SdfPath::Hash> boundMaterialPathsSet(
        boundMaterialPaths.begin(), boundMaterialPaths.end());

    // Invalidate material prims that were added but never bound (until now).
    for (const SdfPath &materialPath : boundMaterialPathsSet) {
        if (!_IsBoundMaterial(materialPath) &&
            _IsTrackedMaterial(materialPath)) {
            newlyBoundEntries.push_back(
                {materialPath, HdDataSourceLocatorSet::UniversalSet()});
        }
    }

    // Update our tracking set of bound materials.
    _boundMaterialPaths.insert(
        boundMaterialPathsSet.begin(), boundMaterialPathsSet.end());

    // Update our tracking set of all material paths.
    _allMaterialPaths.insert(
        addedMaterialPaths.begin(), addedMaterialPaths.end());

    _SendPrimsAdded(entries);
    _SendPrimsDirtied(newlyBoundEntries);
}

void
HdsiUnboundMaterialOverridingSceneIndex::_PrimsRemoved(
    const HdSceneIndexBase &sender,
    const HdSceneIndexObserver::RemovedPrimEntries &entries)
{
    for (const HdSceneIndexObserver::RemovedPrimEntry &entry : entries) {
        _allMaterialPaths.erase(entry.primPath);
        _boundMaterialPaths.erase(entry.primPath);
   }

    _SendPrimsRemoved(entries);
}

void
HdsiUnboundMaterialOverridingSceneIndex::_PrimsDirtied(
    const HdSceneIndexBase &sender,
    const HdSceneIndexObserver::DirtiedPrimEntries &entries)
{
    TRACE_FUNCTION();
    
    // Below, we check if the material binding locators have changed and update
    // our tracking set of bound materials and invalidate newly bound materials
    // we've seen before similar to the logic in _PrimsAdded.
    //
    size_t i = 0;

    while (i < entries.size()) {
        const HdSceneIndexObserver::DirtiedPrimEntry &entry = entries[i];

        if (entry.dirtyLocators.Intersects(_bindingLocators)) {
            break;
        }
        i++;
    }

    // Bindings have not changed.
    if (i == entries.size()) {
        _SendPrimsDirtied(entries);
        return;
    }

    HdSceneIndexObserver::DirtiedPrimEntries newlyBoundEntries;
    for (; i < entries.size(); i++) {
        const HdSceneIndexObserver::DirtiedPrimEntry &entry = entries[i];

        if (entry.dirtyLocators.Intersects(_bindingLocators)) {
            const HdSceneIndexPrim prim =
                _GetInputSceneIndex()->GetPrim(entry.primPath);

            if (prim.primType.IsEmpty()) {
                // Ignore bindings on intermediate prims, like in _PrimsAdded.
                continue;
            }

            const SdfPathVector materialPaths =
                _GetBoundMaterialPaths(prim.dataSource, _bindingPurposes);

            for (const SdfPath &materialPath : materialPaths) {
                if (!_IsBoundMaterial(materialPath)) {
                    if (_IsTrackedMaterial(materialPath)) {
                        newlyBoundEntries.push_back(
                            {materialPath,
                            HdDataSourceLocatorSet::UniversalSet()});
                    }
                    
                    _boundMaterialPaths.insert(materialPath);
                }
            }
        }
    }

    if (newlyBoundEntries.empty()) {
        _SendPrimsDirtied(entries);
        return;
    }

    HdSceneIndexObserver::DirtiedPrimEntries newEntries(entries);
    newEntries.insert(
        newEntries.end(),
        newlyBoundEntries.begin(), newlyBoundEntries.end());

    _SendPrimsDirtied(newEntries);
}

HdsiUnboundMaterialOverridingSceneIndex::HdsiUnboundMaterialOverridingSceneIndex(
    HdSceneIndexBaseRefPtr const &inputSceneIndex,
    HdContainerDataSourceHandle const &inputArgs)
: HdSingleInputFilteringSceneIndexBase(inputSceneIndex)
, _bindingPurposes(_GetMaterialBindingPurposes(inputArgs))
, _bindingLocators(_ComputeBindingLocators(_bindingPurposes))
{
    _PopulateFromInputSceneIndex();
}

HdsiUnboundMaterialOverridingSceneIndex::
~HdsiUnboundMaterialOverridingSceneIndex() = default;

void
HdsiUnboundMaterialOverridingSceneIndex::_PopulateFromInputSceneIndex()
{
    TRACE_FUNCTION();
    
    // Track all material prim paths to find unbound materials.
    // Having sorted paths (SdfPathSet) lets us use set_difference below.
    SdfPathSet allMaterialPaths;

    for (const SdfPath &primPath :
            HdSceneIndexPrimView(_GetInputSceneIndex())) {

        const HdSceneIndexPrim prim = _GetInputSceneIndex()->GetPrim(primPath);

        if (prim.primType.IsEmpty()) {
            // Ignore bindings on intermdiate prims, similar to _PrimsAdded
            // and _PrimsDirtied.
            continue;
        }

        if (prim.primType == HdPrimTypeTokens->material) {
            allMaterialPaths.insert(primPath);
            continue;
        }

        // Check if this prim has bound materials.
        const SdfPathVector boundMaterialPaths =
            _GetBoundMaterialPaths(prim.dataSource, _bindingPurposes);
        
        if (boundMaterialPaths.empty()) {
            continue;
        }
        
        // Add the bound material paths to our tracking set.
        _boundMaterialPaths.insert(
            boundMaterialPaths.begin(), boundMaterialPaths.end());
    }

    if (!_IsObserved()) {
        // If we are not observed, we don't need to send any dirty notices.
        return;
    }

    // Invalidate unbound materials by sending a dirty notice with the default
    // prim-level locator.
    //
    SdfPathVector unboundMaterialPaths;
    std::set_difference(
        allMaterialPaths.begin(), allMaterialPaths.end(),
       _boundMaterialPaths.begin(),_boundMaterialPaths.end(),
        std::back_inserter(unboundMaterialPaths));
    
    if (!unboundMaterialPaths.empty()) {
        HdSceneIndexObserver::DirtiedPrimEntries dirtiedEntries;
        for (const SdfPath &primPath : unboundMaterialPaths) {
            dirtiedEntries.emplace_back(
                primPath, HdDataSourceLocatorSet::UniversalSet());
        }
        _SendPrimsDirtied(dirtiedEntries);
    }
}

bool
HdsiUnboundMaterialOverridingSceneIndex::_IsBoundMaterial(
    const SdfPath &primPath) const
{
    return _Contains(_boundMaterialPaths, primPath);
}

bool
HdsiUnboundMaterialOverridingSceneIndex::_IsTrackedMaterial(
    const SdfPath &primPath) const
{
    return _Contains(_allMaterialPaths, primPath);
}

PXR_NAMESPACE_CLOSE_SCOPE