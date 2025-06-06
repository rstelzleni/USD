//
// Copyright 2025 Pixar
//
// Licensed under the terms set forth in the LICENSE.txt file available at
// https://openusd.org/license.
#ifndef PXR_IMAGING_HDSI_UNBOUND_MATERIAL_OVERRIDING_SCENE_INDEX_H
#define PXR_IMAGING_HDSI_UNBOUND_MATERIAL_OVERRIDING_SCENE_INDEX_H

#include "pxr/pxr.h"

#include "pxr/imaging/hdsi/api.h"
#include "pxr/imaging/hd/filteringSceneIndex.h"
#include "pxr/imaging/hd/dataSourceLocator.h"

#include "pxr/base/tf/declarePtrs.h"
#include "pxr/usd/sdf/path.h"
#include "pxr/base/vt/array.h"

#include <unordered_set>

PXR_NAMESPACE_OPEN_SCOPE

#define HDSI_UNBOUND_MATERIAL_OVERRIDING_SCENE_INDEX_TOKENS \
    (materialBindingPurposes)

TF_DECLARE_PUBLIC_TOKENS(HdsiUnboundMaterialOverridingSceneIndexTokens, HDSI_API,
                         HDSI_UNBOUND_MATERIAL_OVERRIDING_SCENE_INDEX_TOKENS);

TF_DECLARE_WEAK_AND_REF_PTRS(HdsiUnboundMaterialOverridingSceneIndex);

///
/// A scene index that nullifies the prim data source for material prims that 
/// are not bound.
///
/// The material binding purposes can be specified via a HdTokenArrayDataSource
/// for the `materialBindingPurposes` token in the input args.
/// If no binding purposes are specified, the scene index will leave unbound
/// materials as is.
///
/// \note We use "overriding" instead of "pruning" in the name to indicate that
///       the scene index does *not* prune prims by means of removal or clearing
///       both the prim type and data source.
///       Instead, only the prim data source is overridden to null for both
///       simplicity and minimal tracking and processing.
///
class HdsiUnboundMaterialOverridingSceneIndex final
    : public HdSingleInputFilteringSceneIndexBase
{
public:
    HDSI_API
    static HdsiUnboundMaterialOverridingSceneIndexRefPtr
    New(HdSceneIndexBaseRefPtr const &inputSceneIndex,
        HdContainerDataSourceHandle const &inputArgs);

public: // HdSceneIndex overrides
    HDSI_API
    HdSceneIndexPrim GetPrim(const SdfPath &primPath) const override;
    HDSI_API
    SdfPathVector GetChildPrimPaths(const SdfPath &primPath) const override;

protected: // HdSingleInputFilteringSceneIndexBase overrides
    HDSI_API
    void _PrimsAdded(
        const HdSceneIndexBase &sender,
        const HdSceneIndexObserver::AddedPrimEntries &entries) override;
    HDSI_API
    void _PrimsRemoved(
        const HdSceneIndexBase &sender,
        const HdSceneIndexObserver::RemovedPrimEntries &entries) override;
    HDSI_API
    void _PrimsDirtied(
        const HdSceneIndexBase &sender,
        const HdSceneIndexObserver::DirtiedPrimEntries &entries) override;

private:
    HDSI_API
    HdsiUnboundMaterialOverridingSceneIndex(
        HdSceneIndexBaseRefPtr const &inputSceneIndex,
        HdContainerDataSourceHandle const &inputArgs);
    HDSI_API
    ~HdsiUnboundMaterialOverridingSceneIndex() override;

    // Traverse input scene to update internal tracking and
    // discover and invalidate unbound materials.
    void _PopulateFromInputSceneIndex();

    bool _IsBoundMaterial(const SdfPath &primPath) const;
    bool _IsTrackedMaterial(const SdfPath &primPath) const;

    const VtArray<TfToken> _bindingPurposes;
    const HdDataSourceLocatorSet _bindingLocators;

    std::unordered_set<SdfPath, SdfPath::Hash> _allMaterialPaths;
    std::unordered_set<SdfPath, SdfPath::Hash> _boundMaterialPaths;
};

PXR_NAMESPACE_CLOSE_SCOPE

#endif
