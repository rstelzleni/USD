#include "meshUtil.h"

#include "fvarTopologyTracker.h"
#include "computationUtil.h"
#include "material.h"
#include "meshTopology.h"
#include "primUtil.h"
#include "resourceRegistry.h"
#include "subdivision.h"

#include "pxr/imaging/hd/sceneDelegate.h"
#include "pxr/imaging/hd/mesh.h"
#include "pxr/imaging/hd/vtBufferSource.h"
#include "pxr/imaging/hf/diagnostic.h"

PXR_NAMESPACE_OPEN_SCOPE

// for debugging
TF_DEFINE_ENV_SETTING(HD_ENABLE_FORCE_QUADRANGULATE, 0,
                      "Apply quadrangulation for all meshes for debug");

// default to use packed normals
TF_DEFINE_ENV_SETTING(HD_ENABLE_PACKED_NORMALS, 1,
                      "Use packed normals");

namespace {

namespace ComputationUtil = HydraPassthroughComputationUtil;
namespace PrimUtil = HydraPassthroughPrimUtil;

bool
_IsEnabledForceQuadrangulate()
{
    static bool enabled = (TfGetEnvSetting(HD_ENABLE_FORCE_QUADRANGULATE) == 1);
    return enabled;
}

bool
_MaterialHasPtex(
    const HdRenderIndex &renderIndex, 
    const SdfPath &materialId)
{
    const HydraPassthroughMaterial *material = 
        static_cast<const HydraPassthroughMaterial *>(
        renderIndex.GetSprim(HdPrimTypeTokens->material, materialId));

    return (material && material->HasPtex());
}

//-------------------------------------------------
// Primvar stuff

// Enqueues a quadrangulation computation to 'computations' for the primvar data
// in 'source',
void
_QuadrangulatePrimvar(HdBufferSourceSharedPtr const &source,
                      HydraPassthroughMeshTopology *topology,
                      SdfPath const &id,
                      ComputationUtil::ComputationComputeQueuePairVector *computations,
                      HydraPassthroughResourceRegistry *resourceRegistry,
                      HdInterpolation interpolation)
                      //HydraPassthroughResourceRegistrySharedPtr const &resourceRegistry)
{
    if (!TF_VERIFY(computations)) {
        return;
    }
    auto computation = 
        topology->GetQuadrangulateComputation(source, id);
            //source->GetName(), source->GetTupleType().type, id);
    // computation can be null for all quad mesh.
    // XXX In HdSt this is a locally defined computation type, and goes into
    // a computations list. In our case it's a buffer source, so I'm not
    // sure if it should go here or not...
    if (computation) {
        printf("Mesh Util: Adding quadrangulation computation for primvar %s, interpolation %s\n",
            source->GetName().GetText(), TfEnum::GetDisplayName(interpolation).c_str());
        resourceRegistry->AddPrimvarSource(id, computation, interpolation);
        //computations->emplace_back(computation, _RefinePrimvarCompQueue);
    }
}

/*
HdBufferSourceSharedPtr
_QuadrangulateFaceVaryingPrimvar(
    HdBufferSourceSharedPtr const &source,
    HydraPassthroughMeshTopology *topology,
    SdfPath const &id,
    HydraPassthroughResourceRegistry *resourceRegistry)
    //HydraPassthroughResourceRegistrySharedPtr const &resourceRegistry)
{
    // note: currently we don't support GPU facevarying quadrangulation.

    // set quadrangulation as source instead of original source.
    HdBufferSourceSharedPtr quadSource =
        topology->GetQuadrangulateFaceVaryingComputation(source, id);

    // don't transfer source to gpu, it needs to be quadrangulated.
    // but it still has to be resolved, so add it to registry.
    resourceRegistry->AddPrimvarSource(source);

    return quadSource;
}

HdBufferSourceSharedPtr
_TriangulateFaceVaryingPrimvar(HdBufferSourceSharedPtr const &source,
                               HydraPassthroughMeshTopology *topology,
                               SdfPath const &id,
                               HydraPassthroughResourceRegistry *resourceRegistry)
                               //HydraPassthroughResourceRegistrySharedPtr const &resourceRegistry)
{
    HdBufferSourceSharedPtr triSource =
        topology->GetTriangulateFaceVaryingComputation(source, id);

    // don't transfer source to gpu, it needs to be triangulated.
    // but it still has to be resolved, so add it to registry.
    resourceRegistry->AddPrimvarSource(source);

    return triSource;
}
*/

// Enqueues a refinement computation to 'computations' for the primvar data
// in 'source',
void
_RefinePrimvar(HdBufferSourceSharedPtr const &source,
               HydraPassthroughMeshTopology *topology,
               SdfPath const &id,
               HydraPassthroughResourceRegistry *resourceRegistry,
               ComputationUtil::ComputationComputeQueuePairVector *computations,
               HdInterpolation interpolation,
               int channel = 0)
{
    if (!TF_VERIFY(computations)) {
        return;
    }
    // CPU subdivision
    //
    // XXX This could go onto sources instead of computations? i guess?
    auto computation =
        topology->GetOsdRefineComputation(
            source, interpolation, channel);
    // computation can be null for empty mesh
    if (computation) {
        printf("Mesh Util: Adding refinement computation for primvar %s, interpolation %s\n",
            source->GetName().GetText(), TfEnum::GetDisplayName(interpolation).c_str());
        resourceRegistry->AddPrimvarSource(id, computation, interpolation);
        //computations->emplace_back(computation, _RefinePrimvarCompQueue);
    }
}

void
_RefineOrQuadrangulateVertexAndVaryingPrimvar(
    HdBufferSourceSharedPtr const &source,
    HydraPassthroughMeshTopology *topology,
    SdfPath const &id,
    bool doRefine,
    bool doQuadrangulate,
    //HydraPassthroughResourceRegistrySharedPtr const &resourceRegistry,
    HydraPassthroughResourceRegistry *resourceRegistry,
    ComputationUtil::ComputationComputeQueuePairVector *computations,
    HdInterpolation interpolation)
{
    printf("RefineOrQuadrangulateVertexAndVaryingPrimvar: id = %s, source name = %s, doRefine = %d, doQuadrangulate = %d, interpolation = %s\n",
        id.GetText(), source->GetName().GetText(), doRefine, doQuadrangulate, TfEnum::GetDisplayName(interpolation).c_str());
    if (doRefine) {
        _RefinePrimvar(source, topology, id,
                       resourceRegistry, computations, interpolation);
    } else if (doQuadrangulate) {
        _QuadrangulatePrimvar(source, topology, id, computations, resourceRegistry, interpolation);
    } 
    // XXX original returns the source, is that cleaner?
}

/*
HdBufferSourceSharedPtr
_RefineOrQuadrangulateOrTriangulateFaceVaryingPrimvar(
    HdBufferSourceSharedPtr source,
    HydraPassthroughMeshTopology *topology,
    SdfPath const &id,
    bool doRefine,
    bool doQuadrangulate,
    HydraPassthroughResourceRegistry *resourceRegistry,
    //HydraPassthroughResourceRegistrySharedPtr const &resourceRegistry,
    ComputationUtil::ComputationComputeQueuePairVector *computations,
    int channel)
{
    //
    // XXX: there is a bug of quad and tris confusion. see bug 121414
    // whut?
    //
    if (doRefine) {
        _RefinePrimvar(source, topology, resourceRegistry, computations, 
                       HydraPassthroughMeshTopology::INTERPOLATE_FACEVARYING,
                       channel);
    } else if (doQuadrangulate) {
        source = _QuadrangulateFaceVaryingPrimvar(source, topology, id, 
                                                  resourceRegistry);
    } else {
        source = _TriangulateFaceVaryingPrimvar(source, topology, id, 
                                                resourceRegistry);
    }

    return source;
}
*/

//-------------------------------------------------

void 
_GatherFaceVaryingTopologies(
        HdRprim const *rprim,
        HdSceneDelegate *sceneDelegate,
//        const HdMeshReprDesc &desc,
//        HdDrawItem *drawItem,
        int geomSubsetDescIndex,
        HdDirtyBits *dirtyBits,
        const SdfPath &id,
        HydraPassthroughMeshTopology *topology,
        HydraPassthroughFvarTopologyTracker *fvarTopologyTracker)
{
    HdPrimvarDescriptorVector primvars = PrimUtil::GetPrimvarDescriptors(
                rprim, sceneDelegate, HdInterpolationFaceVarying);

    if (!primvars.empty()) {
        for (HdPrimvarDescriptor const& primvar: primvars) {
            if (!HdChangeTracker::IsPrimvarDirty(*dirtyBits, id, primvar.name)) {
                continue;
            }
            const int numFaceVaryings = topology->GetNumFaceVaryings();

            VtValue value;
            VtIntArray indices(0);
            if (primvar.indexed) {
                value = sceneDelegate->GetIndexedPrimvar(
                                        id, primvar.name, &indices);

                if (indices.empty()) {
                    //HF_VALIDATION_WARN(id, 
                    //    "Found empty indices for indexed face-varying primvar "
                    //    "%s. Skipping indices update.",
                    //    primvar.name.GetText());
                    continue;
                } else if ((int)indices.size() < numFaceVaryings) {
                    //HF_VALIDATION_WARN(id, 
                    //    "Indices for face-varying primvar %s has only %d " 
                    //    "elements, while its topology expects at least %d "
                    //    "elements. Skipping indices update.", 
                    //    primvar.name.GetText(), (int)indices.size(), 
                    //    numFaceVaryings);
                    continue;
                }
            } else {
                value = sceneDelegate->Get(id, primvar.name);
                for (int i = 0; i < numFaceVaryings; ++i) {
                    indices.push_back(i);
                }
            }
                        
            fvarTopologyTracker->AddOrUpdateTopology(primvar.name, indices);
        }
    }

    // Also check for removed primvars
    /* I'm not sure how these would get removed, but this is based on the HdSt
     * drawItem having it's BAR available. The BAR is checked to see what
     * primvars it requires. For now leaving this disabled in this code,
     * but wanted to keep it around in case I'm missing something.
     *
     * Coming back to this later, I think primvars could have been removed if
     * there is no shader or compute pass that needs them.
     *
    HdBufferSpecVector removedSpecs;
    if (*dirtyBits & HdChangeTracker::DirtyPrimvar) {
        HdBufferArrayRangeSharedPtr const &bar =
            drawItem->GetFaceVaryingPrimvarRange();
        TfTokenVector internallyGeneratedPrimvars; // empty
        removedSpecs = HdStGetRemovedPrimvarBufferSpecs(bar, primvars, 
            internallyGeneratedPrimvars, id);
    }

    for (size_t i = 0; i < removedSpecs.size(); ++i) {
        TfToken const& removedName = removedSpecs[i].name;
        fvarTopologyTracker->RemovePrimvar(removedName);
    }
    */
    
    fvarTopologyTracker->RemoveUnusedTopologies();
}
} // anonymous namespace

namespace HydraPassthroughMeshUtil
{

void PopulateMeshTopology(
    HdRprim const* rprim,
    SdfPath const& id,
    HdSceneDelegate* sceneDelegate,
    HdDirtyBits* dirtyBits,
    bool updateSubdivTags,
    bool updateDisplayStyle,
    bool updatePrimvars,
    HydraPassthroughMeshTopology* finalTopology,
    HydraPassthroughFvarTopologyTracker* fvarTopologyTracker
    )
{
    if (!TF_VERIFY(finalTopology != nullptr)) {
        return;
    }

    // In HdSt the scene delegate is queried for the render delegate,
    // which then has the resource registry, which is notified of changes
    // to toplogy objects. We don't have this structure in the passthrough
    // render delegate, we rely on the HdResourceRegistry, not the
    // HdStResourceRegistry, which is what layers in the change tracking

    // Get the topology from the scene delegate
    HdMeshTopology topology = sceneDelegate->GetMeshTopology(id);
    HdDisplayStyle const displayStyle = sceneDelegate->GetDisplayStyle(id);

    // In other render delegates, when pulling a new topology, they take
    // extra staps to avoid overwriting refine level or subdiv tags, 
    // which are provided separately by the scene delegate. They save and
    // restore those settings here. In our case we're not expecting these
    // settings to be changing dynamically, so I don't save/restore them 
    // here.
    //
    // We do need to manage these settings though.
    if (updateSubdivTags) {
        if (topology.GetRefineLevel() > 0) {
            topology.SetSubdivTags(sceneDelegate->GetSubdivTags(id));
        }
    }
    if (updateDisplayStyle) {
        // I'm not sure why we set refine level here after checking it for
        // subdiv tags above, but doing so to be consistent with other
        // delegates.
        topology = HdMeshTopology(topology, displayStyle.refineLevel);
    }

    // Now set up our passthrough mesh topology
    int refineLevel = displayStyle.refineLevel;
    HydraPassthroughMeshTopology::RefineMode refineMode =
            HydraPassthroughMeshTopology::RefineModeUniform;
//        _limitNormals = false;
//
//        _flatShadingEnabled = displayStyle.flatShadingEnabled;
//        _displacementEnabled = displayStyle.displacementEnabled;
//        _displayInOverlay = displayStyle.displayInOverlay;
//        _occludedSelectionShowsThrough =
//            displayStyle.occludedSelectionShowsThrough;
//        _pointsShadingEnabled = displayStyle.pointsShadingEnabled;
//
    HdMeshTopology meshTopology =  sceneDelegate->GetMeshTopology(id);

    // Topological visibility (of points, faces) comes in as DirtyTopology.
    // We encode this information in a separate BAR.
//        if (dirtyTopology) {
//            HdStProcessTopologyVisibility(
//                meshTopology.GetInvisibleFaces(),
//                meshTopology.GetNumFaces(),
//                meshTopology.GetInvisiblePoints(),
//                meshTopology.GetNumPoints(),
//                &_sharedData,
//                drawItem,
//                renderParam,
//                &changeTracker,
//                resourceRegistry,
//                id);    
//        }

    // If flat shading is enabled for this prim, make sure we're computing
    // flat normals. It's ok to set the dirty bit here because it's a
    // custom (non-scene) dirty bit, and DirtyTopology will propagate to
    // DirtyPoints if we're computing CPU normals (since flat normals
    // computation requires points data).
//        if (_flatShadingEnabled) {
//            if (!(_customDirtyBitsInUse & DirtyFlatNormals)) {
//                _customDirtyBitsInUse |= DirtyFlatNormals;
//                *dirtyBits |= DirtyFlatNormals;
//            }
//        }
          
    // If the topology requires none subdivision scheme then force
    // refinement level to be 0 since we do not want subdivision.
    if (meshTopology.GetScheme() == PxOsdOpenSubdivTokens->none) {
        refineLevel = 0;
    }

    // If the topology supports adaptive refinement and that's what this
    // prim wants, note that and also that our normals will be generated
    // in the shader.
//        if (meshTopology.GetScheme() != PxOsdOpenSubdivTokens->bilinear &&
//            meshTopology.GetScheme() != PxOsdOpenSubdivTokens->none &&
//            refineLevel > 0 &&
//            _UseLimitRefinement(sceneDelegate->GetRenderIndex(), meshTopology)) {
//            refineMode = HdSt_MeshTopology::RefineModePatches;
//            _limitNormals = true;
//        }

    *finalTopology =
        std::move(HydraPassthroughMeshTopology(meshTopology, refineLevel, refineMode,
            HydraPassthroughMeshTopology::QuadsTriangulated));
    
    // Gather and sanitize geom subsets
//        const HdGeomSubsets &oldGeomSubsets = _topology ? 
//            _topology->GetGeomSubsets() : HdGeomSubsets();
//        const HdGeomSubsets &geomSubsets = topology->GetGeomSubsets();
//        // This will handle draw item creation/update for all existing reprs.
//        if (geomSubsets != oldGeomSubsets) {
//            _UpdateDrawItemsForGeomSubsets(sceneDelegate, renderParam, drawItem,
//                reprToken, repr, geomSubsets, oldGeomSubsets.size());
//        }

    if (refineLevel > 0) {
        // add subdiv tags before compute hash
        finalTopology->SetSubdivTags(sceneDelegate->GetSubdivTags(id));
    }

    const int geomSubsetDescIndex = 0; // temp, replace with real subset once supported
    TfToken fvarLinearInterpRule =
        finalTopology->GetSubdivTags().GetFaceVaryingInterpolationRule();

    if ((refineLevel > 0) && 
        (fvarLinearInterpRule != PxOsdOpenSubdivTokens->all) && 
        updatePrimvars) { //HdChangeTracker::IsAnyPrimvarDirty(*dirtyBits, id)) {
        _GatherFaceVaryingTopologies(
            rprim, sceneDelegate, geomSubsetDescIndex, 
                dirtyBits, id, finalTopology, fvarTopologyTracker);
        finalTopology->SetFvarTopologies(
            fvarTopologyTracker->GetFvarTopologies());
    }
    
    /*
     * Moved this here from below, then decided to put it back in Mesh
    // if refined, we submit a subdivision preprocessing
    // no matter what desc says
    // (see the lengthy comment in PopulateVertexPrimvar)
    if (refineLevel > 0) {
        // OpenSubdiv preprocessing
        HdBufferSourceSharedPtr
            topologySource = topology->GetOsdTopologyComputation(id);
        resourceRegistry->AddSource(topologySource);
    }

    // we also need quadinfo if requested.
    // Note that this is needed even if refineLevel > 0, in case
    // HdMeshGeomStyleHull is going to be used.
    if (useQuadIndices) {
        // Quadrangulate preprocessing
        HdBufferSourceSharedPtr quadInfoBuilder =
            topology->GetQuadInfoBuilderComputation(id);
        resourceRegistry->AddSource(quadInfoBuilder);
    }
    */


// This entire block creates a hash key, stores the topology in the resource
// registry, and adds some computations to the registry as needed. Our registry
// doesn't support any of these operations, if this is needed I'll need to
// build a resource registry to support it.
//
//        // Compute id here. In the future delegate can provide id directly 
//        // without hashing.
//        _topologyId = topology->ComputeHash();
//
//        // Salt the hash with face-varying topologies
//        for (const auto& it : _fvarTopologyTracker->GetTopologyToPrimvarVector()) {
//            _topologyId = ArchHash64((const char*)it.first.cdata(),
//                                     sizeof(int) * it.first.size(), 
//                                     _topologyId);
//            for (const TfToken& it2 : it.second) {
//                _topologyId = ArchHash64(it2.GetText(), it2.size(),
//                                        _topologyId);
//            }
//        }
//
//        // Salt the hash with refinement level and useQuadIndices.
//        // (refinement level is moved into HdMeshTopology)
//        //
//        // Specifically for quad indices, we could do better here because all we
//        // really need is the ability to compute quad indices late, however
//        // splitting the topology shouldn't be a huge cost either.
//        bool useQuadIndices = _UseQuadIndices(sceneDelegate->GetRenderIndex(), topology);
//        _topologyId = ArchHash64((const char*)&useQuadIndices,
//            sizeof(useQuadIndices), _topologyId);
//
//        {
//            // ask registry if there's a sharable mesh topology
//            HdInstance<HydraPassthroughMeshTopology> topologyInstance =
//                resourceRegistry->RegisterMeshTopology(_topologyId);
//
//            if (topologyInstance.IsFirstInstance()) {
//                // if this is the first instance, set this topology to registry.
//                topologyInstance.SetValue(topology);
//
//                // if refined, we submit a subdivision preprocessing
//                // no matter what desc says
//                // (see the lengthy comment in PopulateVertexPrimvar)
//                if (refineLevel > 0) {
//                    // OpenSubdiv preprocessing
//                    HdBufferSourceSharedPtr
//                        topologySource = topology->GetOsdTopologyComputation(id);
//                    resourceRegistry->AddSource(topologySource);
//                }
//
//                // we also need quadinfo if requested.
//                // Note that this is needed even if refineLevel > 0, in case
//                // HdMeshGeomStyleHull is going to be used.
//                if (useQuadIndices) {
//                    // Quadrangulate preprocessing
//                    HdSt_QuadInfoBuilderComputationSharedPtr quadInfoBuilder =
//                        topology->GetQuadInfoBuilderComputation(
//                            /*GPU*/ true, id, resourceRegistry.get());
//                    resourceRegistry->AddSource(quadInfoBuilder);
//                }
//            }
//            _topology = topologyInstance.GetValue();
//        }
//        TF_VERIFY(_topology);
//
//        // hash collision check
//        if (TfDebug::IsEnabled(HD_SAFE_MODE)) {
//            TF_VERIFY(*topology == *_topology);
//        }

//        _vertexAdjacencyBuilder.reset();

// here, we have _topology up-to-date.

}


void
PopulateVertexPrimvars(
        HdRprim const* rprim,
        SdfPath const& id,
        HdSceneDelegate* sceneDelegate,
        HydraPassthroughResourceRegistry *resourceRegistry,
        HydraPassthroughMeshTopology * topology,
        const HdMeshReprDesc &desc,
        HdDrawItem *drawItem,
        int geomSubsetDescIndex,
        HdDirtyBits *dirtyBits,
        bool requireSmoothNormals)
{
    HD_TRACE_FUNCTION();
    HF_MALLOC_TAG_FUNCTION();

    // The "points" attribute is expected to be in this list.
    HdPrimvarDescriptorVector primvars = PrimUtil::GetPrimvarDescriptors(
            rprim, sceneDelegate, HdInterpolationVertex);

    // Track the last vertex index to distinguish between vertex and varying
    // while processing.
    const int vertexPartitionIndex = int(primvars.size()-1);

    // Add varying primvars so we can process them all together, below.
    /*
     * I don't understand this. We will also process these in the PopulateVaryingPrimvars
     * function, so does this just add a BAR for these under vertex things? I'm leaving
     * this out, but if we figure out we need it can be restored. If we keep it I think
     * we need to keep a separate list of varying and vertex primvars so that we can
     * add them to the output with their correct interpolation.
     *
     * Note if we fully remove this, we can update the loop below to remove the isVarying
     * check too.
    HdPrimvarDescriptorVector varyingPvs = PrimUtil::GetPrimvarDescriptors(
            rprim, sceneDelegate, HdInterpolationVarying);
    primvars.insert(primvars.end(), varyingPvs.begin(), varyingPvs.end());
    */
    printf("XXX Found %zu vertex primvars\n", primvars.size());

    HdExtComputationPrimvarDescriptorVector compPrimvars =
        sceneDelegate->GetExtComputationPrimvarDescriptors(id,
            HdInterpolationVertex);
    printf("XXX Found %zu vertex comp primvars\n", compPrimvars.size());

    HdBufferSourceSharedPtrVector sources;
    HdBufferSourceSharedPtrVector reserveOnlySources;
    HdBufferSourceSharedPtrVector separateComputationSources;
    ComputationUtil::ComputationComputeQueuePairVector computations;
    sources.reserve(primvars.size());

    int numPoints = topology ? topology->GetNumPoints() : 0;
    int refineLevel = topology ? topology->GetRefineLevel() : 0;

    ComputationUtil::GetExtComputationPrimvarsComputations(
        id,
        sceneDelegate,
        compPrimvars,
        *dirtyBits,
        &sources,
        &reserveOnlySources,
        &separateComputationSources,
        &computations);
    
    // XXX Member variables in the old code, see if we need these.
    // Currently, they are all giving warnings about being set but not used
    [[maybe_unused]] HdType _pointsDataType = HdType::HdTypeInvalid;
    [[maybe_unused]] bool _sceneNormals = false;
    [[maybe_unused]] HdInterpolation _sceneNormalsInterpolation = HdInterpolationCount;
    [[maybe_unused]] bool _displayOpacity = false;
    
    bool isPointsComputedPrimvar = false;
    {
        // Update tracked state for points and normals that are computed.
        for (HdBufferSourceSharedPtrVector const& computedSources :
             {reserveOnlySources, sources}) {
            for (HdBufferSourceSharedPtr const& source: computedSources) {
                if (source->GetName() == HdTokens->points) {
                    isPointsComputedPrimvar = true;
                    _pointsDataType = source->GetTupleType().type;
                }
                // XXX Seems to only be used for material binding in HdSt
                //if (source->GetName() == HdTokens->normals) {
                //    _sceneNormalsInterpolation = HdInterpolationVertex;
                //    _sceneNormals = true;
                //}
            }
        }
    }
    
    const bool doRefine = (refineLevel > 0);
    const bool doQuadrangulate = UseQuadIndices(sceneDelegate->GetRenderIndex(),
                                                sceneDelegate->GetMaterialId(id),
                                                topology);

    {
        // sources needed for computations
        for (HdBufferSourceSharedPtr const & source : reserveOnlySources) {
            printf("XXX reserveOnlySources: %s\n", source->GetName().GetText());
            _RefineOrQuadrangulateVertexAndVaryingPrimvar(
                source, topology, id,  doRefine, doQuadrangulate,
                resourceRegistry,
                &computations, 
                HdInterpolationVertex);
        }

        for (HdBufferSourceSharedPtr const & source : sources) {
            printf("XXX sources: %s\n", source->GetName().GetText());
            _RefineOrQuadrangulateVertexAndVaryingPrimvar(
                source, topology, id,  doRefine, doQuadrangulate,
                resourceRegistry,
                &computations, 
                HdInterpolationVertex);
        }
    }

    // Track primvars that are skipped because they have zero elements
    HdPrimvarDescriptorVector zeroElementPrimvars;

    // If any primvars use doubles, we need to know if the Hgi backend supports
    // these, or if they need to be converted to floats.
    const bool doublesSupported = true; //_GetDoubleSupport(resourceRegistry);


    // Track index to identify varying primvars.
    int i = 0;
    for (HdPrimvarDescriptor const& primvar: primvars) {
        // If the index is greater than the last vertex index, isVarying=true.
        bool isVarying = i++ > vertexPartitionIndex;

        if (!HdChangeTracker::IsPrimvarDirty(*dirtyBits, id, primvar.name)) {
            continue;
        }

        VtValue value =  sceneDelegate->Get(id, primvar.name);

        if (!value.IsEmpty()) {
            HdBufferSourceSharedPtr source =
                std::make_shared<HdVtBufferSource>(primvar.name, value, 1,
                                                   doublesSupported);

            if (source->GetNumElements() == 0 &&
                source->GetName() != HdTokens->points) {
                // zero elements for primvars other than points will be treated
                // as if the primvar doesn't exist, so no warning is necessary
                zeroElementPrimvars.push_back(primvar);
                continue;
            }

            // verify primvar length -- it is alright to have more data than we
            // index into; the inverse is when we issue a warning and skip
            // update.
            if ((int)source->GetNumElements() < numPoints) {
                HF_VALIDATION_WARN(id, 
                    "Vertex primvar %s has only %d elements, while"
                    " its topology expects at least %d elements. Skipping "
                    " primvar update.",
                    primvar.name.GetText(),
                    (int)source->GetNumElements(), numPoints);

                if (primvar.name == HdTokens->points) {
                    // If points data is invalid, it pretty much invalidates
                    // the whole prim.
                    HF_VALIDATION_WARN(id,
                        "Points data insufficient for topology. Skipping prim.");
                    return;
                }

                continue;

            } else if ((int)source->GetNumElements() > numPoints) {
                HF_VALIDATION_WARN(id,
                    "Vertex primvar %s has %d elements, while"
                    " its topology references only upto element index %d.",
                    primvar.name.GetText(),
                    (int)source->GetNumElements(), numPoints);

                // If the primvar has more data than needed, we issue a warning,
                // but don't skip the primvar update. Truncate the buffer to
                // the expected length.
                std::static_pointer_cast<HdVtBufferSource>(source)
                    ->Truncate(numPoints);
            }

            if (source->GetName() == HdTokens->normals) {
                _sceneNormalsInterpolation =
                    isVarying ? HdInterpolationVarying : HdInterpolationVertex;
                _sceneNormals = true;
            } else if (source->GetName() == HdTokens->displayOpacity) {
                _displayOpacity = true;
            }

            // Special handling of points primvar.
            // We need to capture state about the points primvar
            // for use with smooth normal computation.
            if (primvar.name == HdTokens->points) {
                if (!TF_VERIFY(!isPointsComputedPrimvar)) {
                    HF_VALIDATION_WARN(id, 
                        "'points' specified as both computed and authored "
                        "primvar. Skipping authored value.");
                    continue;
                }
                _pointsDataType = source->GetTupleType().type;
            }

            printf("XXX processing primvar %s\n", source->GetName().GetText());
            _RefineOrQuadrangulateVertexAndVaryingPrimvar(
                source, topology, id,  doRefine, doQuadrangulate,
                resourceRegistry,
                &computations, isVarying ? 
                    HdInterpolationVarying : HdInterpolationVertex);

            sources.push_back(source);
        }
    }

    // remove the primvars with zero elements from further processing
    for (HdPrimvarDescriptor const& primvar: zeroElementPrimvars) {
        auto pos = std::find(primvars.begin(), primvars.end(), primvar);
        if (pos != primvars.end()) {
            primvars.erase(pos);
        }
    }

    // XXX This is for smooth normal computation. Need to figure out how to
    // enable this, then uncomment, add the SmoothNormalsComputation, and
    // execute it.
//    TfToken generatedNormalsName;
//    if (requireSmoothNormals && (*dirtyBits & DirtySmoothNormals)) {
//        // note: normals gets dirty when points are marked as dirty,
//        // at changetracker.
//
//        // clear DirtySmoothNormals (this is not a scene dirtybit)
//        *dirtyBits &= ~DirtySmoothNormals;
//
//        TF_VERIFY(_vertexAdjacencyBuilder);
//
//        // we can't use packed normals for refined/quad,
//        // let's migrate the buffer to full precision
//        bool usePackedSmoothNormals =
//            IsEnabledPackedNormals() && !(doRefine || doQuadrangulate);
//
//        generatedNormalsName = usePackedSmoothNormals ? 
//            efToken("packedSmoothNormals") : TfToken("smoothNormals");
//        
//        if (_pointsDataType != HdTypeInvalid) {
//            // Smooth normals will compute normals as the same datatype
//            // as points, unless we ask for packed normals.
//            // This is unfortunate; can we force them to be float?
//            HdStComputationSharedPtr smoothNormalsComputation =
//                std::make_shared<HdSt_SmoothNormalsComputationGPU>(
//                    _vertexAdjacencyBuilder.get(),
//                    HdTokens->points,
//                    generatedNormalsName,
//                    _pointsDataType,
//                    usePackedSmoothNormals);
//            computations.emplace_back(
//                smoothNormalsComputation, _NormalsCompQueue);
//
//            // note: we haven't had explicit dependency for GPU
//            // computations just yet. Currently they are executed
//            // sequentially, so the dependency is expressed by
//            // registering order.
//            //
//            // note: we can use "pointsDataType" as the normals data type
//            // because, if we decided to refine/quadrangulate, we will have
//            // forced unpacked normals.
//            if (doRefine) {
//                HdStComputationSharedPtr computation =
//                    _topology->GetOsdRefineComputationGPU(
//                        HdStTokens->smoothNormals, _pointsDataType,
//                        resourceRegistry.get(),
//                        HdSt_MeshTopology::INTERPOLATE_VERTEX);
//
//                // computation can be null for empty mesh
//                if (computation) {
//                    computations.emplace_back(
//                        computation, _RefineNormalsCompQueue);
//                }
//            } else if (doQuadrangulate) {
//                HdStComputationSharedPtr computation =
//                    _topology->GetQuadrangulateComputationGPU(
//                        HdStTokens->smoothNormals,
//                        _pointsDataType, GetId());
//
//                // computation can be null for all-quad mesh
//                if (computation) {
//                    computations.emplace_back(
//                        computation, _RefineNormalsCompQueue);
//                }
//            }
//        }
//    }

    // I deleted much BAR management here

    // schedule buffer sources
    if (!sources.empty()) {
        // add sources to update queue
        printf("Mesh Util: Adding %zu primvar sources for %s\n", sources.size(), id.GetText());
        resourceRegistry->AddPrimvarSources(id, std::move(sources), HdInterpolationVertex);
    }
    // add gpu computations to queue.
//    for (auto const& compQueuePair : computations) {
//        HdStComputationSharedPtr const& comp = compQueuePair.first;
//        HdStComputeQueue queue = compQueuePair.second;
//        resourceRegistry->AddComputation(
//            drawItem->GetVertexPrimvarRange(), comp, queue);
//    }
    if (!separateComputationSources.empty()) {
        for (auto const& src : separateComputationSources) {
            resourceRegistry->AddPrimvarSource(id, src, HdInterpolationVertex);
        }
    }
}

bool
UseQuadIndices(
    const HdRenderIndex &renderIndex,
    const SdfPath &materialId,
    const HydraPassthroughMeshTopology *topology)
{
    // We should never quadrangulate for subdivision schemes
    // which refine to triangles (like Loop)
    if (topology->RefinesToTriangles()) {
        return false;
    }

    // Return true if any bound materials use ptex
    bool materialHasPtex = false;

    materialHasPtex = materialHasPtex ||
        _MaterialHasPtex(renderIndex, materialId);

    const HdGeomSubsets &geomSubsets = topology->GetGeomSubsets();
    for (const HdGeomSubset &geomSubset : geomSubsets) {
        materialHasPtex = materialHasPtex ||
            _MaterialHasPtex(renderIndex, geomSubset.materialId);
    }

    // Fallback to the environment variable, which allows forcing of
    // quadrangulation for debugging/testing.
    return materialHasPtex || _IsEnabledForceQuadrangulate();
}

} // namespace HydraPassthroughMeshUtil

PXR_NAMESPACE_CLOSE_SCOPE
