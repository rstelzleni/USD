//
// Copyright 2025 Pixar
//
// Licensed under the terms set forth in the LICENSE.txt file available at
// https://openusd.org/license.
//
#include "pxr/usdImaging/usdSkelImaging/dataSourceSkeletonPrim.h"

#include "pxr/usdImaging/usdImaging/dataSourceMapped.h"
#include "pxr/usdImaging/usdSkelImaging/skeletonSchema.h"

#include "pxr/usd/usdSkel/skeleton.h"

#include "pxr/imaging/hd/overlayContainerDataSource.h"
#include "pxr/imaging/hd/purposeSchema.h"
#include "pxr/imaging/hd/retainedDataSource.h"
#include "pxr/imaging/hd/tokens.h"

PXR_NAMESPACE_OPEN_SCOPE

namespace {

std::vector<UsdImagingDataSourceMapped::PropertyMapping>
_GetPropertyMappings()
{
    std::vector<UsdImagingDataSourceMapped::PropertyMapping> result;

    for (const TfToken &usdName :
             UsdSkelSkeleton::GetSchemaAttributeNames(
                 /* includeInherited = */ false)) {
        result.push_back(
            UsdImagingDataSourceMapped::AttributeMapping{
                usdName, HdDataSourceLocator(usdName) });
    }

    return result;
}

const UsdImagingDataSourceMapped::PropertyMappings &
_GetMappings() {
    static const UsdImagingDataSourceMapped::PropertyMappings result(
        _GetPropertyMappings(),
        UsdSkelImagingSkeletonSchema::GetDefaultLocator());
    return result;
}

}

// ----------------------------------------------------------------------------

UsdSkelImagingDataSourceSkeletonPrim::UsdSkelImagingDataSourceSkeletonPrim(
        const SdfPath &sceneIndexPath,
        UsdPrim usdPrim,
        const UsdImagingDataSourceStageGlobals &stageGlobals)
    : UsdImagingDataSourceGprim(sceneIndexPath, usdPrim, stageGlobals)
{
}

void
_AddIfNecessary(const TfToken &name, TfTokenVector * const names)
{
    if (std::find(names->begin(), names->end(), name) == names->end()) {
        names->push_back(name);
    }
}

TfTokenVector
UsdSkelImagingDataSourceSkeletonPrim::GetNames()
{
    TfTokenVector result = UsdImagingDataSourceGprim::GetNames();
    result.push_back(UsdSkelImagingSkeletonSchema::GetSchemaToken());

    _AddIfNecessary(HdPurposeSchema::GetSchemaToken(), &result);

    return result;
}

HdDataSourceBaseHandle
UsdSkelImagingDataSourceSkeletonPrim::Get(const TfToken & name)
{
    if (name == UsdSkelImagingSkeletonSchema::GetSchemaToken()) {
        return
            UsdImagingDataSourceMapped::New(
                _GetUsdPrim(),
                _GetSceneIndexPath(),
                _GetMappings(),
                _GetStageGlobals());
    }

    HdDataSourceBaseHandle const result =
        UsdImagingDataSourceGprim::Get(name);

    if (name == HdPurposeSchema::GetSchemaToken()) {
        static HdContainerDataSourceHandle const purposeSchemaDataSource =
            HdPurposeSchema::Builder()
                .SetPurpose(
                    HdRetainedTypedSampledDataSource<TfToken>::New(
                        // This token and the method returning a token
                        // data source should be on HdPurposeSchema.
                        HdRenderTagTokens->guide))
                .Build();

        return HdOverlayContainerDataSource::OverlayedContainerDataSources(
            // Authored opinion about purpose overrides default.
            HdContainerDataSource::Cast(result),
            purposeSchemaDataSource);
    }

    return result;
}

HdDataSourceLocatorSet
UsdSkelImagingDataSourceSkeletonPrim::Invalidate(
    UsdPrim const& prim,
    const TfToken &subprim,
    const TfTokenVector &properties,
    const UsdImagingPropertyInvalidationType invalidationType)
{
    TRACE_FUNCTION();

    HdDataSourceLocatorSet locators =
        UsdImagingDataSourceMapped::Invalidate(
            properties, _GetMappings());

    locators.insert(
        UsdImagingDataSourceGprim::Invalidate(
            prim, subprim, properties, invalidationType));

    return locators;
}

PXR_NAMESPACE_CLOSE_SCOPE
