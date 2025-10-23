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
        | HdChangeTracker::DirtyMaterialId
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
    ss << "  materialId: " << _meshData.materialId.GetText() << std::endl;
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
    ss << "  primvars: {" << std::endl;
    for (auto const& pv : _meshData.primvars) {
        ss << "    " << pv.first.GetText() << ": ";
        ss << TfStringify(pv.second.data);
        ss << " (" << pv.second.interpolation << ")";
        if (!pv.second.role.IsEmpty()) {
            ss << " role=" << pv.second.role.GetText();
        }
        ss << std::endl;
    }
    ss << "  }" << std::endl;
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

    // Scene Delegate is currently a HdSceneIndexAdapterSceneDelegate

    SdfPath const& id = GetId();
    _meshData.id = id;

    ////////////////////////////////////////////////////////////////////////
    // 1. Pull scene data.

    if (*dirtyBits & HdChangeTracker::DirtyMaterialId) {
        _meshData.materialId = sceneDelegate->GetMaterialId(id);
        // Set material id for rprim in Hd
        SetMaterialId(_meshData.materialId);
    }

    TfTokenVector computedPrimvars =
        _UpdateComputedPrimvarSources(sceneDelegate, *dirtyBits);

    // If we didn't get points from a computed primvar, get them from an attr
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
        // In other render delegates, when pulling a new topology, they take
        // extra staps to avoid overwriting refine level or subdiv tags, 
        // which are provided separately by the scene delegate. They save and
        // restore those settings here. In our case we're not expecting these
        // settings to be changing dynamically, so I don't save/restore them 
        // here.
        _meshData.topology = sceneDelegate->GetMeshTopology(id);
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
        HdChangeTracker::IsPrimvarDirty(*dirtyBits, id, HdTokens->widths) ) {
        _UpdatePrimvarSources(sceneDelegate, *dirtyBits);
    }
    */

    if (HdChangeTracker::IsPrimvarDirty(*dirtyBits, id, HdTokens->primvar)) {
        // In HdEmbreeMesh::_UpdatePrimvarSources, they pull primvars that are
        // used in computations, but we want all primvars, so we do this here.
        for (size_t i=0; i < HdInterpolationCount; ++i) {
            HdInterpolation interp = static_cast<HdInterpolation>(i);
            HdPrimvarDescriptorVector primvars =
                sceneDelegate->GetPrimvarDescriptors(id, interp);
            for (HdPrimvarDescriptor const& pv: primvars) {
                _meshData.primvars[pv.name] = {
                    sceneDelegate->Get(id, pv.name),
                    interp,
                    pv.role
                };
            }
        }
    }

    // Now that we're through the top level dirty bits, do derivative
    // computations.
    if (HdChangeTracker::IsTopologyDirty(*dirtyBits, id)) {
        // XXX RYANS This doesn't do open subdiv refinement. I think we'd
        // need to get access to the PxOsdTopology from this topology object
        // and then call some other utilities to make subdivs work.
        //
        // HdStMesh relies heavily on GPU computation for this, we'll likely
        // need to go to Osd apis directly so we can call CPU versions.
        //
        // Note also, HdSt does all this triangulation/quadrangulation in
        // conputations, I'm not sure if there's an advantage there for this
        // use case. See HdSt/triangulate.h and HdSt/quadrangulate.h

        // If the topology is dirty, we need to update the face vertex counts
        // and indices.
        //
        // We're making triangle meshes only currently
        HdMeshUtil meshUtil(&_meshData.topology, GetId());
        meshUtil.ComputeTriangleIndices(
                &_meshData.faceVertexIndices,
                &_meshData.triangleOriginalFaceIndices,
                &_meshData.triangleEdgeIndices);

        // Now we need to process any primvars that depend on the topology
        // For the moment, without subdiv refinement, this is only face-varying
        // primvars, and uvs could be face varying.
        //
        // Note that _meshData.primvarSourceMap may need the same behavior,
        // depending on what the computations are doing
        for (auto & pv : _meshData.primvars) {
            if (pv.second.interpolation == HdInterpolationFaceVarying) {
                VtValue result;
                HdTupleType tupleType = HdGetValueTupleType(pv.second.data);
                if(meshUtil.ComputeTriangulatedFaceVaryingPrimvar(
                    HdGetValueData(pv.second.data),
                    tupleType.count,
                    tupleType.type,
                    &result)) {
                    // replace the primvar data with the triangulated version
                    pv.second.updatedData = result;
                }
                else {
                    TF_WARN("Failed to compute triangulated face-varying primvar for %s",
                            pv.first.GetText());
                }
            }
        }
    }

    // It seems like you should only clear the "Scene" dirty bits because
    // there are other dirty bits we shouldn't process in Sync.
    // See HdStMesh::Sync
    *dirtyBits &= ~HdChangeTracker::AllSceneDirtyBits;
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
                                    (GetId(), interp);

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
        } else {
            _meshData.primvarSourceMap[compPrimvar.name] =
                {it->second, compPrimvar.interpolation};
        }
    }

    return compPrimvarNames;
}


PXR_NAMESPACE_CLOSE_SCOPE
