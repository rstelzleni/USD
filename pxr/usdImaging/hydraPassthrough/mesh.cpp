#include "mesh.h"
#include "meshUtil.h"
#include "primUtil.h"
#include "renderParam.h"
#include "resourceRegistry.h"

#include "pxr/imaging/hd/extComputationUtils.h"
#include "pxr/imaging/hd/meshUtil.h"
#include "pxr/imaging/hf/diagnostic.h"

#include "pxr/base/tf/diagnosticLite.h"

PXR_NAMESPACE_OPEN_SCOPE

namespace PrimUtil = HydraPassthroughPrimUtil;
namespace MeshUtil = HydraPassthroughMeshUtil;

HydraPassthroughMesh::HydraPassthroughMesh(SdfPath const &id) 
    : HdMesh(id)
    , _topology(HdMeshTopology(), 0,
                HydraPassthroughMeshTopology::RefineModeUniform,
                HydraPassthroughMeshTopology::QuadsTriangulated)
{}

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
    
    // Initialize the representation with a default Repr.
    // An rprim owns a shared data object, and it also has potentially 
    // multiple reprs. Each repr can create one or more draw items,  which
    // can share that data. These need to be initialized in _InitRepr using
    // the protected members _reprs and _sharedData.
    _ReprVector::iterator it = std::find_if(_reprs.begin(), _reprs.end(),
                                            _ReprComparator(reprToken));
    bool isNew = it == _reprs.end();

    if (isNew) {
        _reprs.emplace_back(reprToken, std::make_shared<HdRepr>());
        *dirtyBits |= HdChangeTracker::DirtyRepr;
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

    _PopulateMeshValues(sceneDelegate, dirtyBits, desc, repr);

    renderData->AddMesh(GetId(), _meshData);
 }

std::string
HydraPassthroughMesh::ToString() const {
    std::stringstream ss;
    ss << "HydraPassthroughMesh {" << std::endl;
    ss << "  id: " << GetId().GetText() << std::endl;
    ss << "  materialId: " << _meshData.materialId.GetText() << std::endl;
    ss << "  points: " << _meshData.points.Get<VtVec3fArray>().size() << std::endl;
    ss << "  topology: " << _topology.GetScheme().GetText() << std::endl;
    ss << "  faceVertexIndices: " << _meshData.faceVertexIndices.size() << std::endl;
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
        ss << std::endl;
    }
    ss << "  }" << std::endl;
    ss << "}";
    return ss.str();
}

void
HydraPassthroughMesh::_PopulateMeshValues(HdSceneDelegate* sceneDelegate,
                                          HdDirtyBits*     dirtyBits,
                                          HdMeshReprDesc const &desc,
                                          HdReprSharedPtr &repr)
{
    HD_TRACE_FUNCTION();
    HF_MALLOC_TAG_FUNCTION();

    auto const& resourceRegistry = 
        std::static_pointer_cast<HydraPassthroughResourceRegistry>(
        sceneDelegate->GetRenderIndex().GetResourceRegistry());

    // Scene Delegate is currently a HdSceneIndexAdapterSceneDelegate

    SdfPath const& id = GetId();
    _meshData.id = id;
    _meshData.fvarTopologyTracker = &_fvarTopologyTracker;

    ////////////////////////////////////////////////////////////////////////
    // 1. Pull scene data.

    if (*dirtyBits & HdChangeTracker::DirtyMaterialId) {
        _meshData.materialId = sceneDelegate->GetMaterialId(id);
        // Set material id for rprim in Hd
        SetMaterialId(_meshData.materialId);
    }

    if (HdChangeTracker::IsTopologyDirty(*dirtyBits, id)) {
        // It would be good to cache these topology objects, they can repeat in
        // a scene, and especially if we add animation support, they can be
        // reused across frames. Topology has a built in hash function for this.
        MeshUtil::PopulateMeshTopology(
            this,
            id,
            sceneDelegate,
            dirtyBits,
            &_topology,
            &_fvarTopologyTracker);

        // This is an Hd level concept, I'm not sure if its required for our
        // case, but doing it since it is expected
         _sharedData.fvarTopologyToPrimvarVector = 
            _fvarTopologyTracker.GetTopologyToPrimvarVector();

        // Toplogy was dirty, so we need to update the point and
        // primvar computations
        _UpdateTopologyDependentComputations(
                sceneDelegate,
                resourceRegistry,
                desc,
                dirtyBits);
    }

    // Should be handled in constant primvars population
    //if (HdChangeTracker::IsTransformDirty(*dirtyBits, id)) {
        // XXX fixme, handle nested transforms
        // Also, are transforms always floats? should I use doubles?
        //_meshData.transform = GfMatrix4f(sceneDelegate->GetTransform(id));
    //}

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
    */

    // It seems like you should only clear the "Scene" dirty bits because
    // there are other dirty bits we shouldn't process in Sync.
    // See HdStMesh::Sync
    *dirtyBits &= ~HdChangeTracker::AllSceneDirtyBits;
}

void
HydraPassthroughMesh::_UpdateTopologyDependentComputations(
    HdSceneDelegate *sceneDelegate,
    const std::shared_ptr<HydraPassthroughResourceRegistry> &resourceRegistry,
    HdMeshReprDesc const &desc,
    HdDirtyBits*     dirtyBits
    )
{
    // If refined, we submit a subdivision preprocessing no matter what desc says
    if (_topology.GetRefineLevel() > 0) {
        // OpenSubdiv preprocessing
        HdBufferSourceSharedPtr
            topologySource = _topology.GetOsdTopologyComputation(GetId());
        resourceRegistry->AddGenericSource(GetId(), topologySource);
    }

    const auto &id = GetId();
    const bool doQuadrangulate = MeshUtil::UseQuadIndices(
                                                sceneDelegate->GetRenderIndex(),
                                                sceneDelegate->GetMaterialId(id),
                                                &_topology);

    // We also need quadinfo if requested.
    // Note that this is needed even if refineLevel > 0, in case
    // HdMeshGeomStyleHull is going to be used.
    //
    // See HdStMesh::_UseQuadIndices
    if (doQuadrangulate) {
        // Quadrangulate preprocessing
        HdBufferSourceSharedPtr quadInfoBuilder =
            _topology.GetQuadInfoBuilderComputation(id);
        resourceRegistry->AddGenericSource(id, quadInfoBuilder);
    }

    const HdGeomSubsets &geomSubsets = _topology.GetGeomSubsets();

    // Normal case
    if (geomSubsets.empty() || desc.geomStyle == HdMeshGeomStylePoints) {

        HdBufferSourceSharedPtrVector sources;
        HdBufferSourceSharedPtr source;

        if (desc.geomStyle == HdMeshGeomStylePoints) {
            // create coarse point indices
            source = _topology.GetPointsIndexBuilderComputation();
            sources.push_back(source);
        } else if (_topology.GetRefineLevel() > 0) {
            // description can also affect refine level if it indicates we're
            // rendering a hull. We may need to take that into account if we
            // support it. See HdStMesh::_GetRefineLevelForDesc

            // create refined indices, primitiveParam and edgeIndices
            source = _topology.GetOsdIndexBuilderComputation();
            sources.push_back(source);

            // Add computations for face varying primvar indices
            if (_topology.GetSubdivTags().GetFaceVaryingInterpolationRule() !=
                    PxOsdOpenSubdivTokens->all) {
                for (size_t i = 0; 
                        i < _fvarTopologyTracker.GetNumTopologies(); 
                        ++i) {
                    HdBufferSourceSharedPtr fvarIndicesSource = 
                        _topology.GetOsdFvarIndexBuilderComputation(i);
                    sources.push_back(fvarIndicesSource);
                }
            }
        } else if (doQuadrangulate) {
            // not refined = quadrangulate
            // create quad indices, primitiveParam and edgeIndices
            source = _topology.GetQuadIndexBuilderComputation(id);
            sources.push_back(source);
        } else {
            // create triangle indices, primitiveParam and edgeIndices
            source = _topology.GetTriangleIndexBuilderComputation(id);
            sources.push_back(source);  
        }

        resourceRegistry->AddIndexSources(id, std::move(sources));

    } else {
        // XXX will handle later, need test data
        // Geom subsets case
        // See HdStMesh::Sync for the similar if/else to this block, we can base
        // the implementation on that, but using local implementations
        // It will likely require updating our output format as well.
        HF_VALIDATION_WARN(id, "Geom subsets not supported yet");
    }

    // Now populate primvar sources and computations that we need.

    // For instancing, see HdStMesh in the section called "INSTACE PRIMVARS"

    // XXX See also HdStMesh::_PopulateAdjacency for smooth normals
    // In HdStMesh they loop over all reprdescs for the current repr to 
    // see if any require smooth or flat normals. We are not initializing
    // these reprs this way in hydra passthrough, so we will just check our
    // single desc. See HdStMesh::_UpdateRepr for relevant code.
    bool requireSmoothNormals = false;
//    bool requireFlatNormals =  false;
    if (desc.geomStyle != HdMeshGeomStyleInvalid) {
        if (desc.flatShadingEnabled) {
//            requireFlatNormals = true;
        } else {
            requireSmoothNormals = true;
        }
    }

    // temp, replace with real subset once supported
    const int geomSubsetDescIndex = 0;

    /* CONSTANT PRIMVARS, TRANSFORM, EXTENT AND PRIMID */
    if (HdChangeTracker::IsAnyPrimvarDirty(*dirtyBits, id) ||
        HdChangeTracker::IsTransformDirty(*dirtyBits, id) ||
        HdChangeTracker::IsExtentDirty(*dirtyBits, id) ||
        HdChangeTracker::IsPrimIdDirty(*dirtyBits, id)) {

        HdPrimvarDescriptorVector constantPrimvars =
            PrimUtil::GetPrimvarDescriptors(
                    this, sceneDelegate, HdInterpolationConstant);
        
        // XXX we need this draw item to get bounds
        bool hasMirroredTransform = _hasMirroredTransform;
        PrimUtil::PopulateConstantPrimvars(this,
                                     &_sharedData,
                                     sceneDelegate,
                                     resourceRegistry.get(),
                                     nullptr /*drawItem*/,
                                     dirtyBits,
                                     constantPrimvars,
                                     &hasMirroredTransform);

        _hasMirroredTransform = hasMirroredTransform;
        
        // Check if normals are provided as a constant primvar
        for (const HdPrimvarDescriptor& pv : constantPrimvars) {
            if (pv.name == HdTokens->normals) {
                _sceneNormalsInterpolation = HdInterpolationConstant;
                _sceneNormals = true;
            }
        }

        // Also want to check existence (not value) of displayOpacity primvar
        _displayOpacity = _displayOpacity || 
            !sceneDelegate->Get(id, HdTokens->displayOpacity).IsEmpty();
    }

    /* VERTEX AND VARYING PRIMVARS */
    if ((*dirtyBits & HdChangeTracker::NewRepr) ||
        HdChangeTracker::IsAnyPrimvarDirty(*dirtyBits, id)) {
        MeshUtil::PopulateVertexAndVaryingPrimvars(
                this, id, sceneDelegate, resourceRegistry.get(), &_topology,
                desc, nullptr /*drawItem*/, geomSubsetDescIndex, dirtyBits,
                requireSmoothNormals);
    }

    /* FACEVARYING PRIMVARS */
    if (HdChangeTracker::IsAnyPrimvarDirty(*dirtyBits, id)) {
        MeshUtil::PopulateFaceVaryingPrimvars(
                this, id, sceneDelegate, resourceRegistry.get(), &_topology,
                &_fvarTopologyTracker,
                nullptr /*drawItem*/, dirtyBits);
    }

//
//    /* ELEMENT PRIMVARS */
//    if ((requireFlatNormals && (*dirtyBits & DirtyFlatNormals)) ||
//        HdChangeTracker::IsAnyPrimvarDirty(*dirtyBits, id)) {
//        _PopulateElementPrimvars(sceneDelegate,
//                                 renderParam,
//                                 repr,
//                                 desc,
//                                 drawItem,
//                                 geomSubsetDescIndex,
//                                 dirtyBits,
//                                 requireFlatNormals);
//    }
//

}

PXR_NAMESPACE_CLOSE_SCOPE
