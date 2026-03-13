#ifndef PXR_USD_IMAGING_HYDRA_PASSTHROUGH_MESH_UTIL_H
#define PXR_USD_IMAGING_HYDRA_PASSTHROUGH_MESH_UTIL_H

#include "pxr/pxr.h"

#include "pxr/usdImaging/hydraPassthrough/meshTopology.h"
#include "pxr/imaging/hd/sceneDelegate.h"

PXR_NAMESPACE_OPEN_SCOPE

class HydraPassthroughFvarTopologyTracker;
class HydraPassthroughResourceRegistry;
class HydraPassthroughVertexAdjacencyBuilder;
class HdMeshReprDesc;
class HdRenderIndex;

/// \class HydraPassthroughMeshUtil
namespace HydraPassthroughMeshUtil
{
    /// Utility function to populate mesh topology
    ///
    /// This serves a similar function to HdStMesh::_PopulateTopology
    void PopulateMeshTopology(
        HdRprim const* prim,
        SdfPath const& id,
        HdSceneDelegate* sceneDelegate,
        HdDirtyBits* dirtyBits,
        HydraPassthroughMeshTopology* meshTopology,
        HydraPassthroughFvarTopologyTracker* fvarTopologyTracker);

    void PopulateVertexAndVaryingPrimvars(
        HdRprim const* prim,
        SdfPath const& id,
        HdSceneDelegate* sceneDelegate,
        HydraPassthroughResourceRegistry *resourceRegistry,
        HydraPassthroughMeshTopology *topology,
        HydraPassthroughVertexAdjacencyBuilder *vertexAdjacencyBuilder,
        const HdMeshReprDesc &desc,
        HdDrawItem *drawItem,
        int geomSubsetDescIndex,
        HdDirtyBits *dirtyBits,
        bool requireSmoothNormals,
        HdType *outPointsDataType);

    void PopulateFaceVaryingPrimvars(
        HdRprim const* rprim,
        SdfPath const& id,
        HdSceneDelegate* sceneDelegate,
        HydraPassthroughResourceRegistry *resourceRegistry,
        HydraPassthroughMeshTopology * topology,
        HydraPassthroughFvarTopologyTracker* fvarTopologyTracker,
        HdDrawItem *drawItem,
        HdDirtyBits *dirtyBits);

    void PopulateElementPrimvars(
        HdRprim const* rprim,
        SdfPath const& id,
        HdSceneDelegate* sceneDelegate,
        HydraPassthroughResourceRegistry *resourceRegistry,
        HydraPassthroughMeshTopology * topology,
        HdDrawItem *drawItem,
        HdDirtyBits *dirtyBits,
        bool requireFlatNormals,
        HdType pointsDataType);

    bool UseQuadIndices(
        const HdRenderIndex &renderIndex,
        const SdfPath &materialId,
        const HydraPassthroughMeshTopology *topology);

    HdTopology::ID GetTopologyHash(
        const HdMeshTopology *topology,
        const HydraPassthroughFvarTopologyTracker *fvarTopologyTracker,
        bool useQuadIndices);

} // namespace HydraPassthroughMeshUtil

PXR_NAMESPACE_CLOSE_SCOPE

#endif // PXR_USD_IMAGING_HYDRA_PASSTHROUGH_MESH_UTIL_H
