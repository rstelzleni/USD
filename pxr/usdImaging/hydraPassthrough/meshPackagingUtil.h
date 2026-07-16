#ifndef PXR_USD_IMAGING_HYDRA_PASSTHROUGH_MESH_PACKAGING_UTIL_H
#define PXR_USD_IMAGING_HYDRA_PASSTHROUGH_MESH_PACKAGING_UTIL_H

#include "pxr/pxr.h"

#include "pxr/usdImaging/hydraPassthrough/renderData.h"

PXR_NAMESPACE_OPEN_SCOPE

namespace HydraPassthroughMeshPackagingUtil
{
    /// Converts the mesh's geom subsets into draw groups: reorders the
    /// fine primitives (and everything parallel to them) so each subset
    /// occupies a contiguous range of faceVertexIndices, then populates
    /// drawGroups with one entry per non-empty subset plus a trailing
    /// group, bound to the mesh's own material, for faces in no subset.
    ///
    /// No-op for meshes without geom subsets. Run this before DeindexMesh
    /// or WeldMesh; both preserve corner order, so the groups stay valid
    /// across them.
    void
    BuildDrawGroups(HydraPassthroughRenderData::MeshData* meshData);

    /// Rewrites the mesh so that every non-constant primvar holds one value per
    /// corner of faceVertexIndices, in corner order, and faceVertexIndices
    /// becomes the identity. The result needs only a single (trivial) index
    /// buffer, which is what renderers without multi-index support consume.
    void
    DeindexMesh(HydraPassthroughRenderData::MeshData* meshData);

    /// Rewrites the mesh into a single-index layout: every non-constant
    /// primvar, along with the points, holds one value per output vertex,
    /// all addressed by the shared faceVertexIndices buffer.
    ///
    /// Corners weld into one output vertex only where all of their
    /// attributes agree: positions and vertex/varying primvars by vertex
    /// index, refined face-varying primvars by their channel indices,
    /// unrefined face-varying primvars by exact bitwise value equality
    /// (no epsilon), and uniform primvars by source face. Vertices
    /// therefore stay shared everywhere except across genuine seams.
    ///
    /// This produces the same renderer-facing contract as DeindexMesh
    /// (single index buffer, expanded primvars parallel to points) but
    /// with the minimal vertex count; DeindexMesh is its worst case and
    /// is kept as the simple reference implementation. Expanded primvars
    /// report vertex interpolation, since they are indexed alongside the
    /// points.
    void
    WeldMesh(HydraPassthroughRenderData::MeshData* meshData);

} // namespace HydraPassthroughMeshPackagingUtil

PXR_NAMESPACE_CLOSE_SCOPE

#endif // PXR_USD_IMAGING_HYDRA_PASSTHROUGH_MESH_PACKAGING_UTIL_H
