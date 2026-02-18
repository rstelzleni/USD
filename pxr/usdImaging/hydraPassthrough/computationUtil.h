#ifndef PXR_USD_IMAGING_HYDRA_PASSTHROUGH_COMPUTATION_UTIL_H
#define PXR_USD_IMAGING_HYDRA_PASSTHROUGH_COMPUTATION_UTIL_H

#include "pxr/pxr.h"

#include "pxr/usdImaging/hydraPassthrough/computation.h"
#include "pxr/imaging/hd/bufferSource.h"
#include "pxr/imaging/hd/sceneDelegate.h"
#include "pxr/imaging/hd/extComputation.h"

PXR_NAMESPACE_OPEN_SCOPE

namespace HydraPassthroughComputationUtil
{

/// \enum HdStComputeQueue
///
/// Determines the 'compute queue' a computation should be added into.
///
/// We only perform synchronization between queues, not within one queue.
/// In OpenGL terms that means we insert memory barriers between computations
/// of two queues, but not between two computations in the same queue.
///
/// A prim determines the role for each queue based on its local knowledge of
/// compute dependencies. Eg. HdStMesh knows computing normals should wait
/// until the primvar refinement computation has fnished. It can assign one
/// queue to primvar refinement and a following queue for normal computations.
///
enum class ComputeQueue {
    ComputeQueueZero=0,
    ComputeQueueOne,
    ComputeQueueTwo,
    ComputeQueueThree,
    ComputeQueueCount};

using ComputationComputeQueuePairVector = 
    std::vector<std::pair<HydraPassthroughComputationSharedPtr, ComputeQueue>>;

/// Utility function to get computations for primvars.
void GetExtComputationPrimvarsComputations(
    const SdfPath &id,
    HdSceneDelegate *sceneDelegate,
    HdExtComputationPrimvarDescriptorVector const& allCompPrimvars,
    HdDirtyBits dirtyBits,
    HdBufferSourceSharedPtrVector *sources,
    HdBufferSourceSharedPtrVector *reserveOnlySources,
    HdBufferSourceSharedPtrVector *separateComputationSources,
    ComputationComputeQueuePairVector *computations);

} // namespace HydraPassthroughComputationUtil

PXR_NAMESPACE_CLOSE_SCOPE

#endif // PXR_USD_IMAGING_HYDRA_PASSTHROUGH_COMPUTATION_UTIL_H
       
