//
// Copyright 2023 Pixar
//
// Licensed under the terms set forth in the LICENSE.txt file available at
// https://openusd.org/license.

#include "pxr/usdImaging/usdImaging/materialBindingsResolvingSceneIndex.h"

#include "pxr/usdImaging/usdImaging/collectionMaterialBindingSchema.h"
#include "pxr/usdImaging/usdImaging/debugCodes.h"
#include "pxr/usdImaging/usdImaging/directMaterialBindingSchema.h"
#include "pxr/usdImaging/usdImaging/materialBindingSchema.h"
#include "pxr/usdImaging/usdImaging/materialBindingsSchema.h"

#include "pxr/usd/usdShade/tokens.h"

#include "pxr/imaging/hd/materialBindingsSchema.h"
#include "pxr/imaging/hd/materialBindingSchema.h"
#include "pxr/imaging/hd/meshSchema.h"
#include "pxr/imaging/hd/overlayContainerDataSource.h"
#include "pxr/imaging/hd/retainedDataSource.h"

#include "pxr/base/trace/trace.h"

#include <optional>

PXR_NAMESPACE_OPEN_SCOPE

namespace
{

// Container that computes the resolved material binding from the flattened 
// direct material bindings.
//
// XXX The flattened direct binding is returned as the resolved binding.
//     This needs to be updated to factor collection bindings.
// 
class _HdMaterialBindingsDataSource final : public HdContainerDataSource
{
public:
    HD_DECLARE_DATASOURCE(_HdMaterialBindingsDataSource);

    _HdMaterialBindingsDataSource(
        const HdContainerDataSourceHandle &primContainer,
        const HdSceneIndexBaseRefPtr &si,
        const SdfPath &primPath)
    : _primContainer(primContainer)
    , _si(si)
    , _primPath(primPath)
    {}

    TfTokenVector
    GetNames() override
    {
        return UsdImagingMaterialBindingsSchema::GetFromParent(
            _primContainer).GetPurposes();
    }

    HdDataSourceBaseHandle
    Get(const TfToken &name) override
    {
        const TfToken &purpose = name;

        const UsdImagingMaterialBindingVectorSchema bindingVecSchema =
            UsdImagingMaterialBindingsSchema::GetFromParent(
                _primContainer).GetMaterialBindings(purpose);
        
        const SdfPath winningBindingPath =
            _ComputeResolvedMaterialBinding(bindingVecSchema);

        if (TfDebug::IsEnabled(USDIMAGING_MATERIAL_BINDING_RESOLUTION)) {
            if (!winningBindingPath.IsEmpty()) {
                TfDebug::Helper().Msg(
                    "*** Prim <%s>: Resolved material binding for purpose "
                    "\'%s\' is <%s>.\n", _primPath.GetText(),
                    purpose.IsEmpty()? "allPurpose": purpose.GetText(),
                    winningBindingPath.GetText());
            }
        }

        return _BuildHdMaterialBindingDataSource(winningBindingPath);
        
        // Note: If the resolved path is the empty path, we don't fallback to 
        //       checking/returning the binding for the empty (allPurpose)
        //       token, with the rationale being that a downstream scene index 
        //       plugin enumerates the strength of the material binding purposes
        //       using for e.g. HdsiMaterialBindingResolvingSceneIndex.
    }

private:

    SdfPath
    _ComputeResolvedMaterialBinding(
        const UsdImagingMaterialBindingVectorSchema &bindingVecSchema) const
    {
        TRACE_FUNCTION();

        // The input is a vector of {direct, collection} binding pairs.
        // The elements are ordered as in a DFS traversal with ancestors
        // appearing before descendants. So, if we find a binding with a
        // strongerThanDescendants strength, we can skip the rest of the
        // bindings.
        // XXX: Add support for collection material bindings.
        //
        SdfPath winningBindingPath;
        int winningBindingIdx = -1;

        for (size_t i = 0; i < bindingVecSchema.GetNumElements(); i++) {
            const UsdImagingMaterialBindingSchema bindingSchema =
                bindingVecSchema.GetElement(i);
            
            const UsdImagingDirectMaterialBindingSchema dirBindingSchema =
                bindingSchema.GetDirectMaterialBinding();
            
            if (!dirBindingSchema) {
                continue;
            }

            auto dirBindingMatPathDs = dirBindingSchema.GetMaterialPath();
            auto dirBindingStrengthDs = dirBindingSchema.GetBindingStrength();

            if (dirBindingMatPathDs && dirBindingStrengthDs) {
                const SdfPath dirMatPath =
                    dirBindingMatPathDs->GetTypedValue(0.0);
                
                if (dirBindingStrengthDs->GetTypedValue(0.0) ==
                        UsdShadeTokens->strongerThanDescendants) {
                    
                    TF_DEBUG(USDIMAGING_MATERIAL_BINDING_RESOLUTION).Msg(
                        "Prim <%s>: Winning material set to <%s>. "
                        "Binding strength for direct binding "
                        "is strongerThanDescendants. "
                        "Skipping the rest of the bindings.\n",
                        _primPath.GetText(), dirMatPath.GetText());
                
                    winningBindingPath = dirMatPath;
                    break;

                } else {
                    if (int(i) > winningBindingIdx) {
                        TF_DEBUG(USDIMAGING_MATERIAL_BINDING_RESOLUTION).Msg(
                         "Prim <%s>: Current winning material set to <%s> "
                         "because the direct binding is more local.\n",
                         _primPath.GetText(), dirMatPath.GetText());

                        winningBindingPath = dirMatPath;
                        winningBindingIdx = i;
                    }
                    // We still need to iterate over the rest of the bindings.
                }
            }
        }
  
        return winningBindingPath;
    }

    static HdDataSourceBaseHandle
    _BuildHdMaterialBindingDataSource(const SdfPath &materialPath)
    {
        return
            materialPath.IsEmpty()
            ? nullptr
            : HdMaterialBindingSchema::Builder()
                .SetPath(HdRetainedTypedSampledDataSource<SdfPath>::New(
                    materialPath))
                .Build();
    }

private:
    HdContainerDataSourceHandle _primContainer;
    
    const HdSceneIndexBaseRefPtr _si; // currently unused, but will be used for
                                      // collection membership queries.
    const SdfPath _primPath;
};

// Prim container override that provides the resolved hydra material bindings
// if direct or collection USD material bindings are present.
// 
class _PrimDataSource final : public HdContainerDataSource
{
public:
    HD_DECLARE_DATASOURCE(_PrimDataSource);

    _PrimDataSource(
        const HdContainerDataSourceHandle &primContainer,
        const HdSceneIndexBaseRefPtr &si,
        const SdfPath &primPath)
    : _primContainer(primContainer)
    , _si(si)
    , _primPath(primPath)
    {}

    TfTokenVector
    GetNames() override
    {
        TfTokenVector names = _primContainer->GetNames();
        names.push_back(HdMaterialBindingsSchema::GetSchemaToken());
        return names;
    }

    HdDataSourceBaseHandle
    Get(const TfToken &name) override
    {
        HdDataSourceBaseHandle result = _primContainer->Get(name);

        // Material bindings on the prim.
        if (name == HdMaterialBindingsSchema::GetSchemaToken()) {

            // Check if we have USD material bindings on the prim to
            // avoid returning an empty non-null container.
            if (UsdImagingMaterialBindingsSchema::GetFromParent(
                _primContainer)) {
                // We don't expect to have hydra material bindings on the
                // prim container. Use an overlay just in case such that the
                // existing opinion wins.
                return
                    HdOverlayContainerDataSource::New(
                        HdContainerDataSource::Cast(result),
                        _HdMaterialBindingsDataSource::New(
                            _primContainer, _si, _primPath));
            }
        }

        return result;
    }

private:
    HdContainerDataSourceHandle _primContainer;
    const HdSceneIndexBaseRefPtr _si;
    const SdfPath _primPath;
};

}

// -----------------------------------------------------------------------------
// UsdImagingMaterialBindingsResolvingSceneIndex
// -----------------------------------------------------------------------------

UsdImagingMaterialBindingsResolvingSceneIndexRefPtr
UsdImagingMaterialBindingsResolvingSceneIndex::New(
    const HdSceneIndexBaseRefPtr &inputSceneIndex,
    const HdContainerDataSourceHandle &inputArgs)
{
    return TfCreateRefPtr(
        new UsdImagingMaterialBindingsResolvingSceneIndex(
            inputSceneIndex, inputArgs));
}

UsdImagingMaterialBindingsResolvingSceneIndex::
UsdImagingMaterialBindingsResolvingSceneIndex(
    const HdSceneIndexBaseRefPtr &inputSceneIndex,
    const HdContainerDataSourceHandle &inputArgs)
  : HdSingleInputFilteringSceneIndexBase(inputSceneIndex)
{
}

UsdImagingMaterialBindingsResolvingSceneIndex::
~UsdImagingMaterialBindingsResolvingSceneIndex() = default;

HdSceneIndexPrim
UsdImagingMaterialBindingsResolvingSceneIndex::GetPrim(
    const SdfPath &primPath) const
{
    TRACE_FUNCTION();

    // Wrap the prim container to provide the resolved hydra bindings via
    // the "materialBindings" locator.
    HdSceneIndexPrim prim = _GetInputSceneIndex()->GetPrim(primPath);
    if (prim.dataSource) {
        prim.dataSource =
            _PrimDataSource::New(
                prim.dataSource, _GetInputSceneIndex(), primPath);
    }

    return prim;
}

SdfPathVector
UsdImagingMaterialBindingsResolvingSceneIndex::GetChildPrimPaths(
    const SdfPath &primPath) const
{
    // This scene index does not mutate the topology.
    return _GetInputSceneIndex()->GetChildPrimPaths(primPath);
}

void
UsdImagingMaterialBindingsResolvingSceneIndex::_PrimsAdded(
    const HdSceneIndexBase &sender,
    const HdSceneIndexObserver::AddedPrimEntries &entries)
{
    TRACE_FUNCTION();

    // For now, just forward the notices. We could suppress notices
    // for USD material bindings schemata locators since scene indices
    // downstream shouldn't be interested in these notices.
    //
    _SendPrimsAdded(entries);
}

void
UsdImagingMaterialBindingsResolvingSceneIndex::_PrimsRemoved(
    const HdSceneIndexBase &sender,
    const HdSceneIndexObserver::RemovedPrimEntries &entries)
{
    TRACE_FUNCTION();

    // Comments above in _PrimsAdded are relevant here.
    _SendPrimsRemoved(entries);
}

void
UsdImagingMaterialBindingsResolvingSceneIndex::_PrimsDirtied(
    const HdSceneIndexBase &sender,
    const HdSceneIndexObserver::DirtiedPrimEntries &entries)
{
    TRACE_FUNCTION();

    // Check if the notice entries can be forwarded as-is.
    bool hasDirtyUsdMaterialBindings = false;
    for (auto const &entry : entries) {
        if (entry.dirtyLocators.Intersects(
                UsdImagingMaterialBindingsSchema::GetDefaultLocator())) {
            hasDirtyUsdMaterialBindings = true;
            break;
        }
    }

    if (!hasDirtyUsdMaterialBindings) {
        _SendPrimsDirtied(entries);
        return;
    }

    // Transform dirty notices for USD material bindings into ones for
    // Hydra material bindings. This effectively suppresses the former notices,
    // which is fine because downstream consumers should work off the
    // Hydra material binding notices.
    //
    HdSceneIndexObserver::DirtiedPrimEntries newEntries;
    for (auto const &entry : entries) {
         if (entry.dirtyLocators.Intersects(
            UsdImagingMaterialBindingsSchema::GetDefaultLocator())) {

            HdDataSourceLocatorSet newLocators(entry.dirtyLocators);
            newLocators = newLocators.ReplacePrefix(
                UsdImagingMaterialBindingsSchema::GetDefaultLocator(),
                HdMaterialBindingsSchema::GetDefaultLocator());

            newEntries.push_back({entry.primPath, newLocators});
        } else {
            newEntries.push_back(entry);
        }
    }

    _SendPrimsDirtied(newEntries);
}


PXR_NAMESPACE_CLOSE_SCOPE
