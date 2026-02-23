#ifndef PXR_USD_IMAGING_HYDRA_PASSTHROUGH_PRIM_UTIL_H
#define PXR_USD_IMAGING_HYDRA_PASSTHROUGH_PRIM_UTIL_H

#include "pxr/pxr.h"

#include "pxr/imaging/hd/drawItem.h"
#include "pxr/imaging/hd/mesh.h"
#include "pxr/imaging/hd/repr.h"
#include "pxr/imaging/hd/rprim.h"
#include "pxr/imaging/hd/sceneDelegate.h"

PXR_NAMESPACE_OPEN_SCOPE

class HydraPassthroughResourceRegistry;

namespace HydraPassthroughPrimUtil
{

// Get potentially filtered primvar descriptors for drawItem
HdPrimvarDescriptorVector
GetPrimvarDescriptors(
    HdRprim const * prim,
    HdSceneDelegate * delegate,
    HdInterpolation interpolation);

void
PopulateConstantPrimvars(
    HdRprim *prim,
    HdRprimSharedData *sharedData,
    HdSceneDelegate *delegate,
    HydraPassthroughResourceRegistry *resourceRegistry,
    HdDrawItem *drawItem,
    HdDirtyBits *dirtyBits,
    HdPrimvarDescriptorVector const& constantPrimvars,
    bool *hasMirroredTransform);

} // namespace HydraPassthroughPrimUtil

PXR_NAMESPACE_CLOSE_SCOPE

#endif // PXR_USD_IMAGING_HYDRA_PASSTHROUGH_PRIM_UTIL_H
