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
// Primvar processing

// Enqueues a quadrangulation computation to 'computations' for the primvar data
// in 'source',
void
_QuadrangulatePrimvar(HdBufferSourceSharedPtr const &source,
                      HydraPassthroughMeshTopology *topology,
                      SdfPath const &id,
                      ComputationUtil::ComputationComputeQueuePairVector *computations,
                      HydraPassthroughResourceRegistry *resourceRegistry,
                      HdInterpolation interpolation)
{
    if (!TF_VERIFY(computations)) {
        return;
    }
    auto computation = 
        topology->GetQuadrangulateComputation(source, id);
    // computation can be null for all quad mesh.
    //
    // In HdSt this computation goes into a different queue, and is done on the GPU.
    // In our case we'll queue it up with the other buffer sources
    if (computation) {
        resourceRegistry->AddPrimvarSource(id, computation, interpolation);
    }
}

HdBufferSourceSharedPtr
_QuadrangulateFaceVaryingPrimvar(
    HdBufferSourceSharedPtr const &source,
    HydraPassthroughMeshTopology *topology,
    SdfPath const &id,
    HydraPassthroughResourceRegistry *resourceRegistry)
{
    // set quadrangulation as source instead of original source.
    HdBufferSourceSharedPtr quadSource =
        topology->GetQuadrangulateFaceVaryingComputation(source, id);

    resourceRegistry->AddPrimvarSource(id, source, HdInterpolationFaceVarying);

    return quadSource;
}

HdBufferSourceSharedPtr
_TriangulateFaceVaryingPrimvar(HdBufferSourceSharedPtr const &source,
                               HydraPassthroughMeshTopology *topology,
                               SdfPath const &id,
                               HydraPassthroughResourceRegistry *resourceRegistry)
{
    HdBufferSourceSharedPtr triSource =
        topology->GetTriangulateFaceVaryingComputation(source, id);

    resourceRegistry->AddPrimvarSource(id, source, HdInterpolationFaceVarying);

    return triSource;
}

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
    auto computation = topology->GetOsdRefineComputation(
            source, interpolation, channel);
    // computation can be null for empty mesh
    if (computation) {
        resourceRegistry->AddPrimvarSource(id, computation, interpolation);
    }
}

void
_RefineOrQuadrangulateVertexAndVaryingPrimvar(
    HdBufferSourceSharedPtr const &source,
    HydraPassthroughMeshTopology *topology,
    SdfPath const &id,
    bool doRefine,
    bool doQuadrangulate,
    HydraPassthroughResourceRegistry *resourceRegistry,
    ComputationUtil::ComputationComputeQueuePairVector *computations,
    HdInterpolation interpolation)
{
    if (doRefine) {
        _RefinePrimvar(source, topology, id,
                       resourceRegistry, computations, interpolation);
    } else if (doQuadrangulate) {
        _QuadrangulatePrimvar(source, topology, id, computations, resourceRegistry, interpolation);
    } 
    // The original code returns the source here instead of adding it to the resource
    // registry, is that cleaner? Could be.
}

HdBufferSourceSharedPtr
_RefineOrQuadrangulateOrTriangulateFaceVaryingPrimvar(
    HdBufferSourceSharedPtr source,
    HydraPassthroughMeshTopology *topology,
    SdfPath const &id,
    bool doRefine,
    bool doQuadrangulate,
    HydraPassthroughResourceRegistry *resourceRegistry,
    ComputationUtil::ComputationComputeQueuePairVector *computations,
    int channel)
{
    //
    // XXX: there is a bug of quad and tris confusion. see bug 121414
    // This comment is from HdSt, I'm not sure what it refers to, but I
    // kept it around
    //
    if (doRefine) {
        _RefinePrimvar(source, topology, id, resourceRegistry, computations, 
                       HdInterpolationFaceVarying, channel);
    } else if (doQuadrangulate) {
        source = _QuadrangulateFaceVaryingPrimvar(source, topology, id,
                                                  resourceRegistry);
    } else {
        source = _TriangulateFaceVaryingPrimvar(source, topology, id, 
                                                resourceRegistry);
    }

    return source;
}

// End primvar processing section
//-------------------------------------------------

void 
_GatherFaceVaryingTopologies(
        HdRprim const *rprim,
        HdSceneDelegate *sceneDelegate,
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
                    HF_VALIDATION_WARN(id, 
                        "Found empty indices for indexed face-varying primvar "
                        "%s. Skipping indices update.",
                        primvar.name.GetText());
                    continue;
                } else if ((int)indices.size() < numFaceVaryings) {
                    HF_VALIDATION_WARN(id, 
                        "Indices for face-varying primvar %s has only %d " 
                        "elements, while its topology expects at least %d "
                        "elements. Skipping indices update.", 
                        primvar.name.GetText(), (int)indices.size(), 
                        numFaceVaryings);
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

    // The original code can remove primvars that are not used by shaders, but
    // we don't do so at the moment. If we do it can be done here, and then
    // cleaned up with the below call.
    
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
    HydraPassthroughMeshTopology* finalTopology,
    HydraPassthroughFvarTopologyTracker* fvarTopologyTracker)
{
    if (!TF_VERIFY(finalTopology != nullptr)) {
        return;
    }

    // Get the topology from the scene delegate
    HdDisplayStyle const displayStyle = sceneDelegate->GetDisplayStyle(id);

    const bool updatePrimvars = HdChangeTracker::IsAnyPrimvarDirty(*dirtyBits, id);

    // In other render delegates, when pulling a new topology, they take
    // extra staps to avoid overwriting refine level or subdiv tags, 
    // which are provided separately by the scene delegate. They save and
    // restore those settings here. In our case we're not expecting these
    // settings to be changing dynamically, so I don't save/restore them 
    // here.

    // Now set up our passthrough mesh topology
    int refineLevel = displayStyle.refineLevel;
    HydraPassthroughMeshTopology::RefineMode refineMode =
            HydraPassthroughMeshTopology::RefineModeUniform;

    // Get another copy of the mesh toplogy, I assume because this will be the
    // original before we made the above changes.
    HdMeshTopology meshTopology =  sceneDelegate->GetMeshTopology(id);

    // If we support topological visibility we can do that here.
    // See HdStProcessTopologyVisibility for reference

    // If the topology requires "none" subdivision scheme then force
    // refinement level to be 0 since we do not want subdivision.
    if (meshTopology.GetScheme() == PxOsdOpenSubdivTokens->none) {
        refineLevel = 0;
    }

    // We are not currently supporting adaptive refinement, because we don't
    // have access to the viewport. If we do support it in the future, this
    // is where this is done in HdStMesh.

    // Set the output topology object
    *finalTopology =
        std::move(HydraPassthroughMeshTopology(meshTopology, refineLevel, refineMode,
            HydraPassthroughMeshTopology::QuadsTriangulated));
    
    // HdSt creates draw items for geom subsets here

    if (refineLevel > 0) {
        finalTopology->SetSubdivTags(sceneDelegate->GetSubdivTags(id));
    }

    const int geomSubsetDescIndex = 0; // temp, replace with real subset once supported
    TfToken fvarLinearInterpRule =
        finalTopology->GetSubdivTags().GetFaceVaryingInterpolationRule();

    if ((refineLevel > 0) && 
        (fvarLinearInterpRule != PxOsdOpenSubdivTokens->all) && 
        updatePrimvars) {

        _GatherFaceVaryingTopologies(
            rprim, sceneDelegate, geomSubsetDescIndex, 
                dirtyBits, id, finalTopology, fvarTopologyTracker);
        finalTopology->SetFvarTopologies(
            fvarTopologyTracker->GetFvarTopologies());
    }
}

void
PopulateVertexAndVaryingPrimvars(
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
    auto vertexPartitionIndex = primvars.size()-1;

    // Add varying primvars so we can process them all together, below.
    HdPrimvarDescriptorVector varyingPvs = PrimUtil::GetPrimvarDescriptors(
            rprim, sceneDelegate, HdInterpolationVarying);
    primvars.insert(primvars.end(), varyingPvs.begin(), varyingPvs.end());

    HdExtComputationPrimvarDescriptorVector compPrimvars =
        sceneDelegate->GetExtComputationPrimvarDescriptors(id,
            HdInterpolationVertex);

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
                }
                if (source->GetName() == HdTokens->normals) {
                    _sceneNormalsInterpolation = HdInterpolationVertex;
                    _sceneNormals = true;
                }
            }
        }
    }
    
    const bool doRefine = (refineLevel > 0);
    const bool doQuadrangulate = UseQuadIndices(sceneDelegate->GetRenderIndex(),
                                                sceneDelegate->GetMaterialId(id),
                                                topology);

    {
        // sources needed for computations
        // Note if these are GPU computations they will not run, but CPU computations will.
        for (HdBufferSourceSharedPtr const & source : reserveOnlySources) {
            _RefineOrQuadrangulateVertexAndVaryingPrimvar(
                source, topology, id,  doRefine, doQuadrangulate,
                resourceRegistry,
                &computations, 
                HdInterpolationVertex);
        }

        for (HdBufferSourceSharedPtr const & source : sources) {
            _RefineOrQuadrangulateVertexAndVaryingPrimvar(
                source, topology, id,  doRefine, doQuadrangulate,
                resourceRegistry,
                &computations, 
                HdInterpolationVertex);
        }
    }

    // Track primvars that are skipped because they have zero elements
    HdPrimvarDescriptorVector zeroElementPrimvars;

    // Don't convert doubles to floats
    const bool doublesSupported = true;

    // Track index to identify varying primvars.
    int i = 0;
    for (HdPrimvarDescriptor const& primvar: primvars) {

        // At this point we have access to primvar.role, but the HdVtBufferSource
        // doesn't hold onto that value. If we want the role in the output data
        // we could derive from HdVtBufferSource and track that data in the new
        // class, then extract it in RenderData. Not doing for now, because role
        // is currently unused in our renderer

        // If the index is greater than the last vertex index, isVarying=true.
        bool isVarying = i++ > int(vertexPartitionIndex);

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
            }

            _RefineOrQuadrangulateVertexAndVaryingPrimvar(
                source, topology, id,  doRefine, doQuadrangulate,
                resourceRegistry,
                &computations, isVarying ? 
                    HdInterpolationVarying : HdInterpolationVertex);

            // If we're refining the course source has already been added.
            if (!doRefine) {
                sources.push_back(source);
            }
        }
    }

    // remove the primvars with zero elements from further processing
    for (HdPrimvarDescriptor const& primvar: zeroElementPrimvars) {
        auto pos = std::find(primvars.begin(), primvars.end(), primvar);
        if (pos != primvars.end()) {
            primvars.erase(pos);
            if (pos <= primvars.begin() + vertexPartitionIndex) {
                --vertexPartitionIndex;
            }
        }
    }

    // Smooth normals could be computed here if we add the smooth normals
    // computations. See HdSt_SmoothNormalsComputationGPU for reference.
    // This is where we'd make use of the _sceneNormals local variable.

    // schedule buffer sources
    if (!sources.empty()) {
        // add sources to update queue
        HdBufferSourceSharedPtrVector vertexSources(
            sources.begin(), sources.begin() + vertexPartitionIndex + 1);
        HdBufferSourceSharedPtrVector varyingSources(
            sources.begin() + vertexPartitionIndex + 1, sources.end());
        resourceRegistry->AddPrimvarSources(id, std::move(vertexSources), HdInterpolationVertex);
        resourceRegistry->AddPrimvarSources(id, std::move(varyingSources), HdInterpolationVarying);
    }

    // Can't currently schedule computations
    if (!computations.empty()) {
        HF_VALIDATION_WARN(id,
                "Computations are not supported yet, skipping %d computations.",
                (int)computations.size());
    }

    if (!separateComputationSources.empty()) {
        for (auto const& src : separateComputationSources) {
            resourceRegistry->AddPrimvarSource(id, src, HdInterpolationVertex);
        }
    }
}

void
PopulateFaceVaryingPrimvars(
        HdRprim const* rprim,
        SdfPath const& id,
        HdSceneDelegate* sceneDelegate,
        HydraPassthroughResourceRegistry *resourceRegistry,
        HydraPassthroughMeshTopology * topology,
        HydraPassthroughFvarTopologyTracker* fvarTopologyTracker,
        HdDrawItem *drawItem,
        HdDirtyBits *dirtyBits)
{
    HD_TRACE_FUNCTION();
    HF_MALLOC_TAG_FUNCTION();

    HdPrimvarDescriptorVector primvars =
        PrimUtil::GetPrimvarDescriptors(
            rprim, sceneDelegate, HdInterpolationFaceVarying);

    if (primvars.empty()) {
        return;
    }

    HdBufferSourceSharedPtrVector sources;
    sources.reserve(primvars.size());
    ComputationUtil::ComputationComputeQueuePairVector computations;

    int refineLevel = topology ? topology->GetRefineLevel() : 0;
    int numFaceVaryings = topology ? topology->GetNumFaceVaryings() : 0;

    TfToken fvarLinearInterpRule =
        topology->GetSubdivTags().GetFaceVaryingInterpolationRule();
    
    // Fvar primvars only need to be refined when the fvar linear interpolation 
    // rule is not "linear all"
    const bool doRefine = (refineLevel > 0 && 
        fvarLinearInterpRule != PxOsdOpenSubdivTokens->all);
    // At higher levels of refinement that do not require full OSD primvar 
    // refinement, we might want to quadrangulate instead
    const bool doQuadrangulate =
        UseQuadIndices(sceneDelegate->GetRenderIndex(),
                       sceneDelegate->GetMaterialId(id),
                       topology) ||
        (refineLevel > 0 && !topology->RefinesToTriangles());

    // Track primvars that are skipped because they have zero elements
    HdPrimvarDescriptorVector zeroElementPrimvars;

    // Don't convert doubles to floats
    const bool doublesSupported = true;

    // XXX Member variables in the old code, see if we need these.
    [[maybe_unused]] bool _sceneNormals = false;
    [[maybe_unused]] HdInterpolation _sceneNormalsInterpolation = HdInterpolationCount;
    [[maybe_unused]] bool _displayOpacity = false;
 
    for (HdPrimvarDescriptor const& primvar: primvars) {
        if (!HdChangeTracker::IsPrimvarDirty(*dirtyBits, id, primvar.name)) {
            continue;
        }

        VtValue value;
        // If refining and primvar is indexed, get unflattened primvar
        const bool useUnflattendPrimvar = doRefine && primvar.indexed;
        if (useUnflattendPrimvar) {
            VtIntArray indices(0);
            value = sceneDelegate->GetIndexedPrimvar(id, primvar.name, &indices);
        } else {
            value = sceneDelegate->Get(id, primvar.name);
        }
        
        if (!value.IsEmpty()) {
            HdBufferSourceSharedPtr source =
                std::make_shared<HdVtBufferSource>(primvar.name, value, 1,
                                                   doublesSupported);

            if (!useUnflattendPrimvar && source->GetNumElements() == 0) {
                // zero elements for primvars will be treated as if the primvar
                // doesn't exist, so no warning is necessary
                zeroElementPrimvars.push_back(primvar);
                continue;
            }

            // verify primvar length
            if ((int)source->GetNumElements() != numFaceVaryings && 
                !useUnflattendPrimvar) {
                HF_VALIDATION_WARN(id, 
                    "# of facevaryings mismatch (%d != %d)"
                    " for primvar %s",
                    (int)source->GetNumElements(), numFaceVaryings,
                    primvar.name.GetText());
                continue;
            }

            if (source->GetName() == HdTokens->normals) {
                _sceneNormalsInterpolation = HdInterpolationFaceVarying;
                _sceneNormals = true;
            } else if (source->GetName() == HdTokens->displayOpacity) {
                _displayOpacity = true;
            }

            int channel = 0;
            if (doRefine) {
                channel = 
                    fvarTopologyTracker->GetChannelFromPrimvar(primvar.name);

                // Invalid fvar topologies may have been skipped when
                // processed by _GatherFaceVaryingTopologies() in which
                // case a validation warning will have been posted already
                // and we should skip further refinement here.
                if (channel < 0) {
                    continue;
                }
            }

            source = _RefineOrQuadrangulateOrTriangulateFaceVaryingPrimvar(
                source, topology, id,  doRefine, doQuadrangulate, 
                resourceRegistry, &computations, channel);
            
            // If doRefine is true, we already added this source in the previous
            // function, but added it as a coarse source for computation
            if (!doRefine) {
                sources.push_back(source);
            }
        }
    }

    // remove the primvars with zero elements from further processing
    for (HdPrimvarDescriptor const& primvar: zeroElementPrimvars) {
        auto pos = std::find(primvars.begin(), primvars.end(), primvar);
        if (pos != primvars.end()) {
            primvars.erase(pos);
        }
    }

    if (!sources.empty()) {
        resourceRegistry->AddPrimvarSources(id, std::move(sources), HdInterpolationFaceVarying);
    }

    // If there are computations to be done, and we haven't converted them
    // into buffer sources, just warn. We don't have a GPU to do these
    // computations in this form.
    if (!computations.empty()) {
        HF_VALIDATION_WARN(id,
            "Face-varying primvar computations are not supported in this render "
            "delegate. Computations will be ignored.");
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
