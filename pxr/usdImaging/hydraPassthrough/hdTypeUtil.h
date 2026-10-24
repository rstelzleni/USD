#ifndef PXR_USD_IMAGING_HYDRA_PASSTHROUGH_HD_TYPE_UTIL_H
#define PXR_USD_IMAGING_HYDRA_PASSTHROUGH_HD_TYPE_UTIL_H

#include "pxr/pxr.h"
#include "pxr/imaging/hd/bufferSource.h"
#include "pxr/base/vt/value.h"

PXR_NAMESPACE_OPEN_SCOPE

/// Utility functions for handling hydra types in the passthrough render delegate
namespace HydraPassthroughHdTypeUtil
{
    /// Given the HdTupleType in this buffer source, return a VtValue with the
    /// appropriate Gf type holding the data from this source. 
    ///
    /// Within the source the data is a void*.
    ///
    /// Note that this function requires the source to be a HdVtBufferSource,
    /// but takes a HdBufferSourceSharedPtr for ease of use with the resource
    /// registry. Calling this with a different buffer source type will give
    /// incorrect results.
    ///
    /// A buffer holding a single element looks the same as a non-array
    /// value, so by default it is returned as a scalar. Pass forceArray
    /// when the caller knows the data is an array (index buffers, or
    /// non-constant primvars) to get a one-element VtArray instead.
    VtValue CastRenderDataToCppType(HdBufferSourceSharedPtr const &source,
                                    bool forceArray = false);

} // namespace HydraPassthroughHdTypeUtil

PXR_NAMESPACE_CLOSE_SCOPE

#endif // PXR_USD_IMAGING_HYDRA_PASSTHROUGH_HD_TYPE_UTIL_H
