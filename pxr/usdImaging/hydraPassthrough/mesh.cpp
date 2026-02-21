#include "mesh.h"
#include "meshUtil.h"
#include "primUtil.h"
#include "renderParam.h"
#include "resourceRegistry.h"

#include <iostream>
#include <sstream>

#include "pxr/imaging/hd/extComputationUtils.h"
#include "pxr/imaging/hd/meshUtil.h"

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
//        if (!pv.second.role.IsEmpty()) {
//            ss << " role=" << pv.second.role.GetText();
//        }
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

    ////////////////////////////////////////////////////////////////////////
    // 1. Pull scene data.

    if (*dirtyBits & HdChangeTracker::DirtyMaterialId) {
        _meshData.materialId = sceneDelegate->GetMaterialId(id);
        // Set material id for rprim in Hd
        SetMaterialId(_meshData.materialId);
    }

    /*
    TfTokenVector computedPrimvars =
        _UpdateComputedPrimvarSources(sceneDelegate, dirtyBits);

    // If we didn't get points from a computed primvar, get them from an attr
    bool pointsIsComputed =
        std::find(computedPrimvars.begin(), computedPrimvars.end(),
                  HdTokens->points) != computedPrimvars.end();
    if (!pointsIsComputed &&
        HdChangeTracker::IsPrimvarDirty(*dirtyBits, id, HdTokens->points)) {
        VtValue value = sceneDelegate->Get(id, HdTokens->points);
        if (not value.IsEmpty()) {
            _meshData.points = value;
        }
    }
    */

    // When we add animation we'll want to cache these, as they're unlikely
    // to change frame to frame.
    if (HdChangeTracker::IsTopologyDirty(*dirtyBits, id)) {
        //_meshData.topology = sceneDelegate->GetMeshTopology(id);
        MeshUtil::PopulateMeshTopology(
            this,
            id,
            sceneDelegate,
            dirtyBits, // XXX can get rid of these next params now
            HdChangeTracker::IsSubdivTagsDirty(*dirtyBits, id),
            HdChangeTracker::IsDisplayStyleDirty(*dirtyBits, id),
            HdChangeTracker::IsAnyPrimvarDirty(*dirtyBits, id),
            &_topology,
            &_fvarTopologyTracker
            );

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
                dirtyBits
                );
    }

    
    /*
    if (HdChangeTracker::IsSubdivTagsDirty(*dirtyBits, id) &&
        _meshData.topology.GetRefineLevel() > 0) {
        _meshData.topology.SetSubdivTags(sceneDelegate->GetSubdivTags(id));
    }
    if (HdChangeTracker::IsDisplayStyleDirty(*dirtyBits, id)) {
        HdDisplayStyle const displayStyle = sceneDelegate->GetDisplayStyle(id);
        _meshData.topology = HdMeshTopology(_meshData.topology,
            displayStyle.refineLevel);
    }
    */

    if (HdChangeTracker::IsTransformDirty(*dirtyBits, id)) {
        // XXX fixme, handle nested transforms
        // Also, are transforms always floats? should I use doubles?
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

    /*
     * Below was workarounds for not doing computations on primvars

    if (HdChangeTracker::IsPrimvarDirty(*dirtyBits, id, HdTokens->primvar)) {
        // In HdEmbreeMesh::_UpdatePrimvarSources, they pull primvars that are
        // used in computations, but we want all primvars, so we do this here.
        for (size_t i=0; i < HdInterpolationCount; ++i) {
            HdInterpolation interp = static_cast<HdInterpolation>(i);
            HdPrimvarDescriptorVector primvars =
                sceneDelegate->GetPrimvarDescriptors(id, interp);
            for (HdPrimvarDescriptor const& pv: primvars) {
                if (pv.indexed) {
                    VtIntArray indices;
                    VtValue indexedValue =
                        sceneDelegate->GetIndexedPrimvar(id, pv.name, &indices);
                    _meshData.primvars[pv.name] = {
                        indexedValue,
                        interp,
                        pv.role,
                        indices
                    };
                }
                else {
                    _meshData.primvars[pv.name] = {
                        sceneDelegate->Get(id, pv.name),
                        interp,
                        pv.role
                    };
                }
            }
        }
    }

    // Now that we're through the top level dirty bits, do dependent
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
        HdMeshUtil meshUtil(&_topology, GetId());
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
    // if refined, we submit a subdivision preprocessing
    // no matter what desc says
    // (see the lengthy comment in PopulateVertexPrimvar)
    if (_topology.GetRefineLevel() > 0) {
        // OpenSubdiv preprocessing
        HdBufferSourceSharedPtr
            topologySource = _topology.GetOsdTopologyComputation(GetId());
        resourceRegistry->AddGenericSource(GetId(), topologySource);
    }

    // we also need quadinfo if requested.
    // Note that this is needed even if refineLevel > 0, in case
    // HdMeshGeomStyleHull is going to be used.
    //
    // See HdStMesh::_UseQuadIndices
    /*
    if (useQuadIndices) {
        // Quadrangulate preprocessing
        HdBufferSourceSharedPtr quadInfoBuilder =
            topology->GetQuadInfoBuilderComputation(id);
        resourceRegistry->AddGenericSource(quadInfoBuilder);
    }
    */

    // Sigh, we need to deal with these
    const HdGeomSubsets &geomSubsets = _topology.GetGeomSubsets();

    // Normal case
    if (geomSubsets.empty() || desc.geomStyle == HdMeshGeomStylePoints) {

        // ask again registry if there's a shareable buffer range for the topology
        //HdInstance<HdBufferArrayRangeSharedPtr> rangeInstance =
        //    resourceRegistry->RegisterMeshIndexRange(_topologyId, indexToken);

        //if (rangeInstance.IsFirstInstance()) {
            // if not exists, update actual topology buffer to range.
            // Allocate new one if necessary.
            HdBufferSourceSharedPtrVector sources;
            HdBufferSourceSharedPtr source;

            if (desc.geomStyle == HdMeshGeomStylePoints) {
                // create coarse points indices
                source = _topology.GetPointsIndexBuilderComputation();
                sources.push_back(source);
            } else if (_topology.GetRefineLevel() > 0) {
                // description can also affect refine level if it indicates we're
                // rendering a hull. We may need to take that into account if we
                // support it. See HdStMesh::_GetRefineLevelForDesc

                // create refined indices, primitiveParam and edgeIndices
                source = _topology.GetOsdIndexBuilderComputation();
                sources.push_back(source);

                // Add face-varying indices and patch params to topology BAR if 
                // necessary
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
            } else if (MeshUtil::UseQuadIndices(
                        sceneDelegate->GetRenderIndex(),
                        sceneDelegate->GetMaterialId(GetId()),
                        &_topology)) {
                // not refined = quadrangulate
                // create quad indices, primitiveParam and edgeIndices
                source = _topology.GetQuadIndexBuilderComputation(GetId());
                sources.push_back(source);
            } else {
                // create triangle indices, primitiveParam and edgeIndices
                source = _topology.GetTriangleIndexBuilderComputation(GetId());
                sources.push_back(source);  
            }

            resourceRegistry->AddIndexSources(GetId(), std::move(sources));

            /*
            // initialize buffer array
            //   * indices
            //   * primitiveParam
            //   * fvarIndices (optional)
            //   * fvarPatchParam (optional)
            HdBufferSpecVector bufferSpecs;
            HdBufferSpec::GetBufferSpecs(sources, &bufferSpecs);

            // Set up the usage hints to mark topology as varying if
            // there is a previously set range
            HdBufferArrayUsageHint usageHint = 
                HdBufferArrayUsageHintBitsIndex |
                HdBufferArrayUsageHintBitsStorage;
//            if (drawItem->GetTopologyRange()) {
//                usageHint |= HdBufferArrayUsageHintBitsSizeVarying;
//            }

            // allocate new range
            HdBufferArrayRangeSharedPtr range =
                resourceRegistry->AllocateNonUniformBufferArrayRange(
                        HdTokens->topology, bufferSpecs, usageHint);

            // add sources to update queue
            resourceRegistry->AddSources(range, std::move(sources));
            */

            // save new range to registry
            //rangeInstance.SetValue(range);
        //}

        // If we are updating an existing topology, notify downstream
        // systems of the change
        /*
        HdBufferArrayRangeSharedPtr const& orgRange =
            drawItem->GetTopologyRange();
        HdBufferArrayRangeSharedPtr newRange = rangeInstance.GetValue();

        if (HdStIsValidBAR(orgRange) && (newRange != orgRange)) {
            TF_DEBUG(HD_RPRIM_UPDATED).Msg("%s has varying topology"
                    " (topology index = %d).\n", id.GetText(),
                    drawItem->GetDrawingCoord()->GetTopologyIndex());

            // Setup a flag to say this mesh's topology is varying
            _hasVaryingTopology = true;
        }

        HdStUpdateDrawItemBAR(
                newRange,
                drawItem->GetDrawingCoord()->GetTopologyIndex(),
                &_sharedData,
                renderParam,
                &changeTracker);
                */
    } else {
        // XXX will handle later, need test data
        // Geom subsets case
//        HdBufferSourceSharedPtr indicesSource;
//        HdBufferSourceSharedPtr fvarIndicesSource;
//
//        bool refined = false;
//        bool quadrangulated = false;
//        if (refineLevelForDesc > 0) {
//            // create refined indices, primitiveParam and edgeIndices
//            indicesSource = _topology->GetOsdIndexBuilderComputation();
//            resourceRegistry->AddSource(indicesSource);
//            // Add face-varying indices and patch params to topology BAR if 
//            // necessary
//            if (_topology->GetSubdivTags().GetFaceVaryingInterpolationRule() !=
//                    PxOsdOpenSubdivTokens->all) {
//                for (size_t i = 0; 
//                        i < _fvarTopologyTracker->GetNumTopologies(); 
//                        ++i) {
//                    fvarIndicesSource = 
//                        _topology->GetOsdFvarIndexBuilderComputation(i);
//                    resourceRegistry->AddSource(fvarIndicesSource);
//                }
//            }
//
//            refined = true;
//            if (_topology->GetScheme() == PxOsdOpenSubdivTokens->catmullClark ||
//                    _topology->GetScheme() == PxOsdOpenSubdivTokens->bilinear) {
//                quadrangulated = true;
//            }
//        } else if (_UseQuadIndices(sceneDelegate->GetRenderIndex(), _topology)) {
//            // not refined = quadrangulate
//            // create quad indices, primitiveParam and edgeIndices
//            indicesSource = _topology->GetQuadIndexBuilderComputation(GetId());
//            resourceRegistry->AddSource(indicesSource);
//            quadrangulated = true;
//        } else {
//            // create triangle indices, primitiveParam and edgeIndices
//            indicesSource = _topology->GetTriangleIndexBuilderComputation(GetId());
//            resourceRegistry->AddSource(indicesSource);
//        }
//
//        // If the mesh has been triangulated, quadrangulated, or refined (as 
//        // refined indices are first triangulated or quadrangulated), we 
//        // need to transform the subset's authored face indices, which are 
//        // given in reference to the base faces of the mesh, to the indices
//        // of the triangulated/quadrangulated faces. These buffer source
//        // computations help us do that.
//        HdBufferSourceSharedPtr geomSubsetFaceIndicesHelperSource = 
//            _topology->GetGeomSubsetFaceIndexHelperComputation(
//                    refined, quadrangulated);
//        resourceRegistry->AddSource(geomSubsetFaceIndicesHelperSource);
//
//        if (refined) {
//            _topology->GetOsdBaseFaceToRefinedFacesMapComputation(
//                    resourceRegistry.get());
//        }
//
//        // For original draw item
//        const std::vector<int> *nonSubsetFaces = 
//            _topology->GetNonSubsetFaces();
//        _CreateTopologyRangeForGeomSubset(resourceRegistry, changeTracker, 
//                renderParam, drawItem, indexToken, indicesSource,
//                fvarIndicesSource, geomSubsetFaceIndicesHelperSource,
//                VtIntArray(nonSubsetFaces->begin(), nonSubsetFaces->end()), 
//                refined);
//
//        // For geom subsets draw items
//        const size_t numGeomSubsets = geomSubsets.size();
//        for (size_t i = 0; i < geomSubsets.size(); ++i) {
//            HdGeomSubset geomSubset = geomSubsets[i];
//            HdStDrawItem *subsetDrawItem = static_cast<HdStDrawItem*>(
//                    repr->GetDrawItemForGeomSubset(
//                        geomSubsetDescIndex, numGeomSubsets, i));
//            _CreateTopologyRangeForGeomSubset(resourceRegistry, 
//                    changeTracker, renderParam, subsetDrawItem, indexToken, 
//                    indicesSource, fvarIndicesSource, 
//                    geomSubsetFaceIndicesHelperSource, geomSubset.indices, 
//                    refined);
//        }
//
    }

    // The index computations should be added to the resource registry at this
    // point. Now we need to add primvar computations.
    /* INSTANCE PRIMVARS */
    /*
    _UpdateInstancer(sceneDelegate, dirtyBits);
    HdStUpdateInstancerData(sceneDelegate->GetRenderIndex(),
                            renderParam,
                            this,
                            drawItem,
                            &_sharedData,
                            *dirtyBits);
    
    _displayOpacity = _displayOpacity ||
            HdStIsInstancePrimvarExistentAndValid(
            sceneDelegate->GetRenderIndex(), this, HdTokens->displayOpacity);
            */

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
    const auto id = GetId();

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
        printf("XXX Populating constant primvars for id=%s\n", id.GetText());
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

    /* VERTEX PRIMVARS */
    if ((*dirtyBits & HdChangeTracker::NewRepr) ||
        HdChangeTracker::IsAnyPrimvarDirty(*dirtyBits, id)) {
        printf("XXX Populating vertex primvars for id=%s\n", id.GetText());
        MeshUtil::PopulateVertexPrimvars(
                this, id, sceneDelegate, resourceRegistry.get(), &_topology,
                desc, nullptr /*drawItem*/, geomSubsetDescIndex, dirtyBits,
                requireSmoothNormals);
    }

    /* FACEVARYING PRIMVARS */
    if (HdChangeTracker::IsAnyPrimvarDirty(*dirtyBits, id)) {
        printf("XXX Populating face-varying primvars for id=%s\n", id.GetText());
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

/*
TfTokenVector
HydraPassthroughMesh::_UpdateComputedPrimvarSources(HdSceneDelegate* sceneDelegate,
                                                    HdDirtyBits* dirtyBits)
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
            if (HdChangeTracker::IsPrimvarDirty(*dirtyBits, id, pv.name)) {
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
*/


PXR_NAMESPACE_CLOSE_SCOPE
