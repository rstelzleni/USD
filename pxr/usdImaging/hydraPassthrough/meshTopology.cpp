#include "meshTopology.h"

#include "pxr/usdImaging/hydraPassthrough/subdivision.h"

#include "pxr/imaging/hd/vtBufferSource.h"
#include "pxr/imaging/hd/tokens.h"

PXR_NAMESPACE_OPEN_SCOPE

HydraPassthroughMeshTopology::HydraPassthroughMeshTopology(
        const HdMeshTopology &src,
        int refineLevel,
        RefineMode refineMode,
        QuadsMode quadsMode)
    : HdMeshTopology(src, refineLevel)
    , _refineMode(refineMode)
    , _quadsMode(quadsMode)
    , _refineLevel(refineLevel)
    , _subdivision(nullptr)
{
    SanitizeGeomSubsets();
}

HydraPassthroughMeshTopology::~HydraPassthroughMeshTopology() = default;

void
HydraPassthroughMeshTopology::SetQuadInfo(HdQuadInfo const *quadInfo)
{
    _quadInfo.reset(quadInfo);
}

void
HydraPassthroughMeshTopology::SanitizeGeomSubsets()
{
    const HdGeomSubsets &geomSubsets = GetGeomSubsets();
    if (geomSubsets.empty()) {
        return;
    }
    const size_t numFaces = GetNumFaces();

    // Keep track of faces that are used within the geom subsets
    std::vector<bool> unusedFaces(numFaces, true);
    size_t numUnusedFaces = numFaces;

    HdGeomSubsets sanitizedGeomSubsets;
    for (const HdGeomSubset &geomSubset : geomSubsets) {
        HdGeomSubset sanitizedGeomSubset = geomSubset;

        // We only care about subsets that will with non-empty indices and
        // material id
        const VtIntArray faceIndices = geomSubset.indices;
        if (!faceIndices.empty() && !geomSubset.materialId.IsEmpty()) {                    
            VtIntArray sanitizedFaceIndices;
            for (size_t i = 0; i < faceIndices.size(); ++i) {
                const int index = faceIndices[i];
                // Skip out-of-bound face indices.
                if (index >= (int)numFaces) {
                    TF_WARN("Geom subset index %d is larger than number of "
                        "faces (%d), removing.", index, (int)numFaces);
                    continue;
                }
                sanitizedFaceIndices.push_back(index);
                if (unusedFaces[index]) {
                    unusedFaces[index] = false;
                    numUnusedFaces--;
                } else {
                    // Warn about duplicated face indices.
                    TF_WARN("Face index %d is repeated between geom subsets", 
                        index);;
                }
            }
            sanitizedGeomSubset.indices = sanitizedFaceIndices;
            sanitizedGeomSubsets.push_back(sanitizedGeomSubset);
        }
    }

    _nonSubsetFaces = std::make_unique<std::vector<int>>();
    _nonSubsetFaces->resize(numUnusedFaces);

    if (numUnusedFaces) {
        size_t count = 0;
        for (size_t i = 0; i < unusedFaces.size() && count < numUnusedFaces; 
             ++i) {
            if (unusedFaces[i]) {
                (*_nonSubsetFaces)[count] = i;
                count++;
            }
        }
    }
    
    SetGeomSubsets(sanitizedGeomSubsets);
}

bool
HydraPassthroughMeshTopology::RefinesToTriangles() const
{
    return HydraPassthroughSubdivision::RefinesToTriangles(_topology.GetScheme());
}

bool
HydraPassthroughMeshTopology::RefinesToBSplinePatches() const
{
    return ((IsEnabledAdaptive() || (_refineMode == RefineModePatches)) &&
            HydraPassthroughSubdivision::RefinesToBSplinePatches(_topology.GetScheme()));
}

bool
HydraPassthroughMeshTopology::RefinesToBoxSplineTrianglePatches() const
{
    return ((IsEnabledAdaptive() || (_refineMode == RefineModePatches)) &&
    HydraPassthroughSubdivision::RefinesToBoxSplineTrianglePatches(_topology.GetScheme()));
}


HdBufferSourceSharedPtr
HydraPassthroughMeshTopology::GetPointsIndexBuilderComputation()
{
    // this is simple enough to return the result right away.
    int numPoints = GetNumPoints();
    VtIntArray indices(numPoints);
    for (int i = 0; i < numPoints; ++i) indices[i] = i;

    return std::make_shared<HdVtBufferSource>(
        HdTokens->indices, VtValue(indices));
}

HdBufferSourceSharedPtr
HydraPassthroughMeshTopology::GetTriangleIndexBuilderComputation(
        SdfPath const &id)
{
    return std::make_shared<
        HydraPassthroughTriangleIndexBuilderComputation>(this, id);
}

HdBufferSourceSharedPtr
HydraPassthroughMeshTopology::GetQuadIndexBuilderComputation(
        SdfPath const &id)
{
    return std::make_shared<HydraPassthroughQuadIndexBuilderComputation>(
        this, _quadInfoBuilder.lock(), id);
}

HdBufferSourceSharedPtr
HydraPassthroughMeshTopology::GetOsdIndexBuilderComputation()
{
    HdBufferSourceSharedPtr topologyBuilder = _osdTopologyBuilder.lock();
    return _subdivision->CreateIndexComputation(this, topologyBuilder);
}

HdBufferSourceSharedPtr
HydraPassthroughMeshTopology::GetOsdFvarIndexBuilderComputation(int channel)
{
    HdBufferSourceSharedPtr topologyBuilder = _osdTopologyBuilder.lock();
    return _subdivision->CreateFvarIndexComputation(this, topologyBuilder, channel);
}

HdBufferSourceSharedPtr
HydraPassthroughMeshTopology::GetOsdTopologyComputation(SdfPath const &id)
{
    if (HdBufferSourceSharedPtr builder = _osdTopologyBuilder.lock()) {
        return builder;
    }

    // this has to be the first instance.
    if (!TF_VERIFY(!_subdivision)) return HdBufferSourceSharedPtr();

    // create HydraPassthroughSubdivision
    _subdivision = std::make_unique<HydraPassthroughSubdivision>(_refineLevel);

    if (!TF_VERIFY(_subdivision)) return HdBufferSourceSharedPtr();

    // create a topology computation for HydraPassthroughSubdivision
    HdBufferSourceSharedPtr builder =
        _subdivision->CreateTopologyComputation(this, id);
    _osdTopologyBuilder = builder; // retain weak ptr
    return builder;
}


HydraPassthroughQuadInfoBuilderComputationSharedPtr
HydraPassthroughMeshTopology::GetQuadInfoBuilderComputation(
        SdfPath const &id)
{
    auto builder = std::make_shared<HydraPassthroughQuadInfoBuilderComputation>(this, id);
    
    // store as a weak ptr, we need it for the quadrangulate computation
    // to make a dependency.
    _quadInfoBuilder = builder;

    return builder;
}

HdBufferSourceSharedPtr
HydraPassthroughMeshTopology::GetQuadrangulateComputation(
    HdBufferSourceSharedPtr const &source, SdfPath const &id)
{
    // check if the quad table is already computed as all-quads.
    if (_quadInfo && _quadInfo->IsAllQuads()) {
        // no need of quadrangulation.
        return HdBufferSourceSharedPtr();
    }

    // Make a dependency to quad info, in case if the topology
    // is changing and the quad info hasn't been populated.
    //
    // It can be null for the second or later primvar animation.
    // Don't call GetQuadInfoBuilderComputation instead. It may result
    // unregisterd computation.
    HdBufferSourceSharedPtr quadInfo = _quadInfoBuilder.lock();

    return std::make_shared<HydraPassthroughQuadrangulateComputation>(
        this, source, quadInfo, id);
}

HdBufferSourceSharedPtr
HydraPassthroughMeshTopology::GetQuadrangulateFaceVaryingComputation(
    HdBufferSourceSharedPtr const &source, SdfPath const &id)
{
    return std::make_shared<HydraPassthroughQuadrangulateFaceVaryingComputation>(
        this, source, id);
}

HdBufferSourceSharedPtr
HydraPassthroughMeshTopology::GetTriangulateFaceVaryingComputation(
    HdBufferSourceSharedPtr const &source, SdfPath const &id)
{
    return std::make_shared<HydraPassthroughTriangulateFaceVaryingComputation>(
        this, source, id);
}

HdBufferSourceSharedPtr
HydraPassthroughMeshTopology::GetOsdRefineComputation(
    HdBufferSourceSharedPtr const &source,
    HdInterpolation interpolation,
    int fvarChannel)
{
    // _toplogy is the PxOsdMeshTopology owned by the base class
    if (_topology.GetFaceVertexCounts().size() == 0) return nullptr;

    if (!TF_VERIFY(_subdivision)) {
        TF_CODING_ERROR("GetOsdTopologyComputation should be called before "
                        "GetOsdRefineComputationGPU.");
        return nullptr;
    }

    HdBufferSourceSharedPtr topologyBuilder = _osdTopologyBuilder.lock();
    
    return _subdivision->CreateRefineComputationCPU(
            this, source, topologyBuilder, interpolation, fvarChannel);
}

PXR_NAMESPACE_CLOSE_SCOPE
