#ifndef PXR_USD_IMAGING_HYDRA_PASSTHROUGH_MESH_PACKAGING_UTIL_H
#define PXR_USD_IMAGING_HYDRA_PASSTHROUGH_MESH_PACKAGING_UTIL_H

#include "pxr/pxr.h"

#include "pxr/usdImaging/hydraPassthrough/renderData.h"

PXR_NAMESPACE_OPEN_SCOPE

namespace HydraPassthroughMeshPackagingUtil
{
    /// Rewrites the mesh so that every non-constant primvar holds one value per
    /// corner of faceVertexIndices, in corner order, and faceVertexIndices
    /// becomes the identity. The result needs only a single (trivial) index
    /// buffer, which is what renderers without multi-index support consume.
    void
    DeindexMesh(HydraPassthroughRenderData::MeshData* meshData);

} // namespace HydraPassthroughMeshPackagingUtil

PXR_NAMESPACE_CLOSE_SCOPE

#endif // PXR_USD_IMAGING_HYDRA_PASSTHROUGH_MESH_PACKAGING_UTIL_H
