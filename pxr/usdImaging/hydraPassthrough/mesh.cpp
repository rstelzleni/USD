//
// Copyright 2020 Pixar
//
// Licensed under the terms set forth in the LICENSE.txt file available at
// https://openusd.org/license.
//
#include "mesh.h"

#include <iostream>

#include "pxr/imaging/hd/extComputationUtils.h"

PXR_NAMESPACE_OPEN_SCOPE

HdTinyMesh::HdTinyMesh(SdfPath const &id) : HdMesh(id) {}

HdDirtyBits HdTinyMesh::GetInitialDirtyBitsMask() const {
    // XXX add topology and points dirty bits? Once I do how do I access that data?
    return HdChangeTracker::Clean // clean is 0, so nothing dirty. I'm not sure why people usually start the list with this
//        | HdChangeTracker::InitRepr // what is this for? isn't this always done initially?
        | HdChangeTracker::DirtyPoints
        | HdChangeTracker::DirtyTopology
        | HdChangeTracker::DirtyTransform
        | HdChangeTracker::DirtyVisibility
        | HdChangeTracker::DirtyCullStyle
        | HdChangeTracker::DirtyDoubleSided
        | HdChangeTracker::DirtyDisplayStyle
        | HdChangeTracker::DirtySubdivTags
        | HdChangeTracker::DirtyPrimvar
        | HdChangeTracker::DirtyNormals
//        | HdChangeTracker::DirtyInstancer // no instancer support yet
        ;

}

HdDirtyBits HdTinyMesh::_PropagateDirtyBits(HdDirtyBits bits) const {
    // No dirty bits to add
    return bits;
}

void HdTinyMesh::_InitRepr(TfToken const &reprToken, HdDirtyBits *dirtyBits) {
    std::cout << "=> InitRepr for Tiny Mesh id=" << GetId() << " reprToken="
                << reprToken << " dirtyBits=" << *dirtyBits << std::endl;
    
    // Initialize the representation with a default Repr.
    // An rprim owns a shared data object, and it also has potentially 
    // multiple reprs. Each repr can create one or more draw items,  which
    // can share that data. These need to be initialized in _InitRepr using
    // the protected members _reprs and _sharedData.
    _ReprVector::iterator it = std::find_if(_reprs.begin(), _reprs.end(),
                                            _ReprComparator(reprToken));
    bool isNew = it == _reprs.end();

    if (isNew) {
        std::cout << "Creating new repr for Tiny Mesh id=" << GetId()
                  << " reprToken=" << reprToken << std::endl;

        _reprs.emplace_back(reprToken, std::make_shared<HdRepr>());
        *dirtyBits |= HdChangeTracker::DirtyRepr;
        /*
        // add new repr
        _reprs.emplace_back(reprToken, std::make_shared<HdRepr>());
        HdReprSharedPtr &repr = _reprs.back().second;

        // set dirty bit to say we need to sync a new repr (buffer array
        // ranges may change)
        *dirtyBits |= HdChangeTracker::DirtyRepr;

        HdRepr::DrawItemUniquePtr drawItem =
            std::make_unique<HdDrawItem>(&_sharedData);
        //HdDrawingCoord *drawingCoord = drawItem->GetDrawingCoord();
        repr->AddDrawItem(std::move(drawItem));
        */

    } else {
        std::cout << "Repr already exists for Tiny Mesh id=" << GetId()
                  << " reprToken=" << reprToken << std::endl;
    }
}


void HdTinyMesh::Sync(HdSceneDelegate *sceneDelegate,
                      HdRenderParam *renderParam, HdDirtyBits *dirtyBits,
                      TfToken const &reprToken)
{
    std::cout << "* (multithreaded) Sync Tiny Mesh id=" << GetId() << std::endl;
    auto reprIt = std::find_if(_reprs.begin(), _reprs.end(),
            _ReprComparator(reprToken));
    if (reprIt == _reprs.end()) {
        std::cout << "No repr found for Tiny Mesh id=" << GetId()
            << " reprToken=" << reprToken << std::endl;
        return;
    }
    HdReprSharedPtr &repr = reprIt->second;
    if (!repr) {
        std::cout << "Repr is null for Tiny Mesh id=" << GetId()
            << " reprToken=" << reprToken << std::endl;
        return;
    }
    HdRepr::DrawItemUniquePtr drawItem =
        std::make_unique<HdDrawItem>(&_sharedData);
    repr->AddDrawItem(std::move(drawItem));

    // XXX: A mesh repr can have multiple repr decs; this is done, for example, 
    // when the drawstyle specifies different rasterizing modes between front
    // faces and back faces.
    _MeshReprConfig::DescArray descs = _GetReprDesc(reprToken);
    const HdMeshReprDesc &desc = descs[0];

    _PopulateMeshValues(sceneDelegate, dirtyBits, desc);

    printf("Sync results\n");
    printf("  points: %zu\n", _points.size());
    for (const auto& p : _points) {
        printf("    %f %f %f\n", p[0], p[1], p[2]);
    }
    printf("  topology: %s\n", _topology.GetScheme().GetText());
    printf("  transform:\n");
    printf("    %.3f %.3f %.3f %.3f\n", _transform[0][0], _transform[0][1], _transform[0][2], _transform[0][3]);
    printf("    %.3f %.3f %.3f %.3f\n", _transform[1][0], _transform[1][1], _transform[1][2], _transform[1][3]);
    printf("    %.3f %.3f %.3f %.3f\n", _transform[2][0], _transform[2][1], _transform[2][2], _transform[2][3]);
    printf("    %.3f %.3f %.3f %.3f\n", _transform[3][0], _transform[3][1], _transform[3][2], _transform[3][3]);

}

void
HdTinyMesh::_PopulateMeshValues(HdSceneDelegate* sceneDelegate,
                                HdDirtyBits*     dirtyBits,
                                HdMeshReprDesc const &desc)
{
    HD_TRACE_FUNCTION();
    HF_MALLOC_TAG_FUNCTION();

    SdfPath const& id = GetId();

    ////////////////////////////////////////////////////////////////////////
    // 1. Pull scene data.
    TfTokenVector computedPrimvars =
        _UpdateComputedPrimvarSources(sceneDelegate, *dirtyBits);

    bool pointsIsComputed =
        std::find(computedPrimvars.begin(), computedPrimvars.end(),
                  HdTokens->points) != computedPrimvars.end();
    if (!pointsIsComputed &&
        HdChangeTracker::IsPrimvarDirty(*dirtyBits, id, HdTokens->points)) {
        VtValue value = sceneDelegate->Get(id, HdTokens->points);
        _points = value.Get<VtVec3fArray>();
        //_normalsValid = false;
    }

    if (HdChangeTracker::IsTopologyDirty(*dirtyBits, id)) {
        // When pulling a new topology, we don't want to overwrite the
        // refine level or subdiv tags, which are provided separately by the
        // scene delegate, so we save and restore them.
        PxOsdSubdivTags subdivTags = _topology.GetSubdivTags();
        int refineLevel = _topology.GetRefineLevel();
        _topology = HdMeshTopology(GetMeshTopology(sceneDelegate), refineLevel);
        _topology.SetSubdivTags(subdivTags);
        //_adjacencyValid = false;
    }
    if (HdChangeTracker::IsSubdivTagsDirty(*dirtyBits, id) &&
        _topology.GetRefineLevel() > 0) {
        _topology.SetSubdivTags(sceneDelegate->GetSubdivTags(id));
    }
    if (HdChangeTracker::IsDisplayStyleDirty(*dirtyBits, id)) {
        HdDisplayStyle const displayStyle = sceneDelegate->GetDisplayStyle(id);
        _topology = HdMeshTopology(_topology,
            displayStyle.refineLevel);
    }

    if (HdChangeTracker::IsTransformDirty(*dirtyBits, id)) {
        _transform = GfMatrix4f(sceneDelegate->GetTransform(id));
    }

    if (HdChangeTracker::IsVisibilityDirty(*dirtyBits, id)) {
        // Evidently we need to trigger this on the RPrim base class
        _UpdateVisibility(sceneDelegate, dirtyBits);
    }

    /*
    if (HdChangeTracker::IsCullStyleDirty(*dirtyBits, id)) {
        _cullStyle = GetCullStyle(sceneDelegate);
    }
    if (HdChangeTracker::IsDoubleSidedDirty(*dirtyBits, id)) {
        _doubleSided = IsDoubleSided(sceneDelegate);
    }
    if (HdChangeTracker::IsPrimvarDirty(*dirtyBits, id, HdTokens->normals) ||
        HdChangeTracker::IsPrimvarDirty(*dirtyBits, id, HdTokens->widths) ||
        HdChangeTracker::IsPrimvarDirty(*dirtyBits, id, HdTokens->primvar)) {
        _UpdatePrimvarSources(sceneDelegate, *dirtyBits);
    }
    */

}


TfTokenVector
HdTinyMesh::_UpdateComputedPrimvarSources(HdSceneDelegate* sceneDelegate,
                                          HdDirtyBits dirtyBits)
{
    HD_TRACE_FUNCTION();
    
    SdfPath const& id = GetId();

    // Get all the dirty computed primvars
    HdExtComputationPrimvarDescriptorVector dirtyCompPrimvars;
    for (size_t i=0; i < HdInterpolationCount; ++i) {
        HdExtComputationPrimvarDescriptorVector compPrimvars;
        HdInterpolation interp = static_cast<HdInterpolation>(i);
        compPrimvars = sceneDelegate->GetExtComputationPrimvarDescriptors
                                    (GetId(),interp);

        for (auto const& pv: compPrimvars) {
            if (HdChangeTracker::IsPrimvarDirty(dirtyBits, id, pv.name)) {
                dirtyCompPrimvars.emplace_back(pv);
            }
        }
    }

    if (dirtyCompPrimvars.empty()) {
        return TfTokenVector();
    }
    
    HdExtComputationUtils::ValueStore valueStore
        = HdExtComputationUtils::GetComputedPrimvarValues(
            dirtyCompPrimvars, sceneDelegate);

    TfTokenVector compPrimvarNames;
    // Update local primvar map and track the ones that were computed
    for (auto const& compPrimvar : dirtyCompPrimvars) {
        auto const it = valueStore.find(compPrimvar.name);
        if (!TF_VERIFY(it != valueStore.end())) {
            continue;
        }
        
        compPrimvarNames.emplace_back(compPrimvar.name);
        if (compPrimvar.name == HdTokens->points) {
            _points = it->second.Get<VtVec3fArray>();
            // _normalsValid = false;
        } else {
            _primvarSourceMap[compPrimvar.name] = {it->second,
                                                compPrimvar.interpolation};
        }
    }

    return compPrimvarNames;
}


PXR_NAMESPACE_CLOSE_SCOPE
