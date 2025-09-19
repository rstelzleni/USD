#include "mesh.h"
#include "renderParam.h"

#include <iostream>
#include <sstream>

#include "pxr/imaging/hd/meshUtil.h"
#include "pxr/imaging/hd/extComputationUtils.h"

#include "pxr/base/tf/diagnosticLite.h"

PXR_NAMESPACE_OPEN_SCOPE

HydraPassthroughMesh::HydraPassthroughMesh(SdfPath const &id) : HdMesh(id) {}

HdDirtyBits HydraPassthroughMesh::GetInitialDirtyBitsMask() const {
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

HdDirtyBits HydraPassthroughMesh::_PropagateDirtyBits(HdDirtyBits bits) const {
    // No dirty bits to add
    return bits;
}

void HydraPassthroughMesh::_InitRepr(TfToken const &reprToken, HdDirtyBits *dirtyBits) {
    TF_STATUS("HydraPassthroughMesh::_InitRepr called for id=%s, reprToken=%s",
                GetId().GetText(), reprToken.GetText());
    
    // Initialize the representation with a default Repr.
    // An rprim owns a shared data object, and it also has potentially 
    // multiple reprs. Each repr can create one or more draw items,  which
    // can share that data. These need to be initialized in _InitRepr using
    // the protected members _reprs and _sharedData.
    _ReprVector::iterator it = std::find_if(_reprs.begin(), _reprs.end(),
                                            _ReprComparator(reprToken));
    bool isNew = it == _reprs.end();

    if (isNew) {
        TF_STATUS("Creating new repr for id=%s, reprToken=%s",
                    GetId().GetText(), reprToken.GetText());

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

    }
}


void HydraPassthroughMesh::Sync(HdSceneDelegate *sceneDelegate,
                      HdRenderParam *renderParam, HdDirtyBits *dirtyBits,
                      TfToken const &reprToken)
{
    TF_STATUS("HydraPassthroughMesh::Sync called for id=%s, reprToken=%s",
                GetId().GetText(), reprToken.GetText());
    auto reprIt = std::find_if(_reprs.begin(), _reprs.end(),
            _ReprComparator(reprToken));
    if (reprIt == _reprs.end()) {
        TF_CODING_ERROR("HydraPassthroughMesh::Sync: "
            "reprToken %s not found for id=%s", reprToken.GetText(),
            GetId().GetText());
        return;
    }
    HdReprSharedPtr &repr = reprIt->second;
    if (!repr) {
        TF_CODING_ERROR("HydraPassthroughMesh::Sync: "
            "reprToken %s is null for id=%s", reprToken.GetText(),
            GetId().GetText());
        return;
    }
    HdRepr::DrawItemUniquePtr drawItem =
        std::make_unique<HdDrawItem>(&_sharedData);
    repr->AddDrawItem(std::move(drawItem));

    auto rp = dynamic_cast<HydraPassthroughRenderParam*>(renderParam);
    if (!rp) {
        TF_CODING_ERROR("HydraPassthroughMesh::Sync: "
                  "renderParam is not a HydraPassthroughRenderParam, "
                  "cannot proceed.");
        return;
    }
    HydraPassthroughRenderDataRefPtr renderData = rp->GetRenderData();

    // XXX: A mesh repr can have multiple repr decs; this is done, for example, 
    // when the drawstyle specifies different rasterizing modes between front
    // faces and back faces.
    _MeshReprConfig::DescArray descs = _GetReprDesc(reprToken);
    const HdMeshReprDesc &desc = descs[0];

    _PopulateMeshValues(sceneDelegate, dirtyBits, desc);

    renderData->AddMesh(GetId(), _meshData);
 }

std::string
HydraPassthroughMesh::ToString() const {
    std::stringstream ss;
    ss << "HydraPassthroughMesh {" << std::endl;
    ss << "  id: " << GetId().GetText() << std::endl;
    ss << "  points: " << _meshData.points.size() << std::endl;
    ss << "  topology: " << _meshData.topology.GetScheme().GetText() << std::endl;
    ss << "  faceVertexIndices: " << _meshData.faceVertexIndices.size() << std::endl;
    ss << "  triangleOriginalFaceIndices: " << _meshData.triangleOriginalFaceIndices.size() << std::endl;
    ss << "  triangleEdgeIndices: " << _meshData.triangleEdgeIndices.size() << std::endl;
    ss << "  transform: " << std::endl;
    ss << "    " << _meshData.transform[0][0] << " "<< _meshData.transform[0][1] << " "
       << _meshData.transform[0][2] << " " << _meshData.transform[0][3] << std::endl;
    ss << "    " << _meshData.transform[1][0] << " "<< _meshData.transform[1][1] << " "
       << _meshData.transform[1][2] << " " << _meshData.transform[1][3] << std::endl;
    ss << "    " << _meshData.transform[2][0] << " "<< _meshData.transform[2][1] << " "
       << _meshData.transform[2][2] << " " << _meshData.transform[2][3] << std::endl;
    ss << "    " << _meshData.transform[3][0] << " "<< _meshData.transform[3][1] << " "
       << _meshData.transform[3][2] << " " << _meshData.transform[3][3] << std::endl;
    ss << "  visible: " << (_meshData.visible ? "true" : "false") << std::endl;
    ss << "}";
    return ss.str();
}

void
HydraPassthroughMesh::_PopulateMeshValues(HdSceneDelegate* sceneDelegate,
                                          HdDirtyBits*     dirtyBits,
                                          HdMeshReprDesc const &desc)
{
    HD_TRACE_FUNCTION();
    HF_MALLOC_TAG_FUNCTION();

    SdfPath const& id = GetId();
    _meshData.id = id;

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
        if (not value.IsEmpty()) {
            _meshData.points = value.Get<VtVec3fArray>();
        }
    }

    if (HdChangeTracker::IsTopologyDirty(*dirtyBits, id)) {
        // When pulling a new topology, we don't want to overwrite the
        // refine level or subdiv tags, which are provided separately by the
        // scene delegate, so we save and restore them.
        PxOsdSubdivTags subdivTags = _meshData.topology.GetSubdivTags();
        int refineLevel = _meshData.topology.GetRefineLevel();
        _meshData.topology = HdMeshTopology(GetMeshTopology(sceneDelegate), refineLevel);
        _meshData.topology.SetSubdivTags(subdivTags);
        //_adjacencyValid = false;
    }
    if (HdChangeTracker::IsSubdivTagsDirty(*dirtyBits, id) &&
        _meshData.topology.GetRefineLevel() > 0) {
        _meshData.topology.SetSubdivTags(sceneDelegate->GetSubdivTags(id));
    }
    if (HdChangeTracker::IsDisplayStyleDirty(*dirtyBits, id)) {
        HdDisplayStyle const displayStyle = sceneDelegate->GetDisplayStyle(id);
        _meshData.topology = HdMeshTopology(_meshData.topology,
            displayStyle.refineLevel);
    }

    if (HdChangeTracker::IsTransformDirty(*dirtyBits, id)) {
        _meshData.transform = GfMatrix4f(sceneDelegate->GetTransform(id));
    }

    if (HdChangeTracker::IsVisibilityDirty(*dirtyBits, id)) {
        // Evidently we need to trigger this on the RPrim base class
        _UpdateVisibility(sceneDelegate, dirtyBits);
        _meshData.visible = IsVisible();
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

    // Now that we're through the top level dirty bits, do derivative
    // computations.
    if (HdChangeTracker::IsTopologyDirty(*dirtyBits, id)) {
        // If the topology is dirty, we need to update the face vertex counts
        // and indices.
        //
        // We're making triangle meshes only currently
        HdMeshUtil meshUtil(&_meshData.topology, GetId());
        meshUtil.ComputeTriangleIndices(
                &_meshData.faceVertexIndices,
                &_meshData.triangleOriginalFaceIndices,
                &_meshData.triangleEdgeIndices);
    }

}


TfTokenVector
HydraPassthroughMesh::_UpdateComputedPrimvarSources(HdSceneDelegate* sceneDelegate,
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
            _meshData.points = it->second.Get<VtVec3fArray>();
            // _normalsValid = false;
        } else {
            _meshData.primvarSourceMap[compPrimvar.name] = {it->second,
                                                compPrimvar.interpolation};
        }
    }

    return compPrimvarNames;
}


PXR_NAMESPACE_CLOSE_SCOPE
