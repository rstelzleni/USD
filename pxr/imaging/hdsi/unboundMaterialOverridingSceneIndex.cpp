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

#include "pxr/base/trace/trace.h"

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
    const SdfPathSet &paths,
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
        _IsUnboundMaterial(primPath)) {
        
        // Clear just the prim container. Note that we don't clear the prim type
        // because this simplifies the tracking state necessary and reduces the
        // processing necessary in the notice handlers.
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

    // This can be parallelized if necessary.
    for (const HdSceneIndexObserver::AddedPrimEntry &entry : entries) {
        if (entry.primType.IsEmpty()) {
            // Ignore bindings on intermediate prims (like scopes and xforms) 
            // for whom material bindings are not relevant but present from
            // flattening.
            continue;
        }
        if (entry.primType == HdPrimTypeTokens->material) {
            // Since we don't clear the prim type for unbound materials,
            // we don't need special processing for material notices.
            continue;
        }
        
        const HdSceneIndexPrim prim =
            _GetInputSceneIndex()->GetPrim(entry.primPath);

        const SdfPathVector materialPaths =
            _GetBoundMaterialPaths(prim.dataSource, _bindingPurposes);

        _boundMaterialPaths.insert(
            materialPaths.begin(), materialPaths.end());
    }

    _SendPrimsAdded(entries);
}

void
HdsiUnboundMaterialOverridingSceneIndex::_PrimsRemoved(
    const HdSceneIndexBase &sender,
    const HdSceneIndexObserver::RemovedPrimEntries &entries)
{
    // We could update our tracking to erase bound material paths that have been
    // removed. For now, keep it simple and do nothing.
    _SendPrimsRemoved(entries);
}

void
HdsiUnboundMaterialOverridingSceneIndex::_PrimsDirtied(
    const HdSceneIndexBase &sender,
    const HdSceneIndexObserver::DirtiedPrimEntries &entries)
{
    TRACE_FUNCTION();
    
    // Below, we check if the material binding locators have changed and update
    // our tracking set of bound materials.
    // We don't suppress the notices for unbound materials.
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
                if (_IsUnboundMaterial(materialPath)) {
                    // This material was not bound before and so its prim
                    // query would have returned an empty prim container.
                    // Invalidate the prim to trigger a re-query...
                    newlyBoundEntries.push_back(
                        {materialPath, HdDataSourceLocatorSet::UniversalSet()});
                    
                    // ... and update our tracking set of bound materials.
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
HdsiUnboundMaterialOverridingSceneIndex::_IsUnboundMaterial(
    const SdfPath &primPath) const
{
    return !_Contains(_boundMaterialPaths, primPath);
}

PXR_NAMESPACE_CLOSE_SCOPE