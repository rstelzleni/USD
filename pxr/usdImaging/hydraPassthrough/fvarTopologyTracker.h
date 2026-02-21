#ifndef PXR_USD_IMAGING_HYDRA_PASSTHROUGH_FVAR_TOPOLOGY_TRACKER_H
#define PXR_USD_IMAGING_HYDRA_PASSTHROUGH_FVAR_TOPOLOGY_TRACKER_H

#include "pxr/pxr.h"

#include "pxr/imaging/hd/rprimSharedData.h"

#include "pxr/base/vt/array.h"
#include "pxr/base/tf/token.h"

PXR_NAMESPACE_OPEN_SCOPE

// For some reason this class doesn't follow the naming conventions used
// elsewhere in USD. Renaming to follow conventions and make this more
// clear for the future, and in case it changes to follow conventions.
using HdTopologyToPrimvarVector = TopologyToPrimvarVector;

/// Helper class for meshes to keep track of the topologies of their
/// face-varying primvars. The face-varying topologies are later passed to 
/// the OSD refiner in an order that will correspond to their face-varying 
/// channel number. We keep a vector of only the topologies in use, paired
/// with their associated primvar names.
///
/// Based on the similar class in HdStMesh
class HydraPassthroughFvarTopologyTracker 
{
public:
    const HdTopologyToPrimvarVector & GetTopologyToPrimvarVector() const {
        return _topologies;
    } 

    /// Add a primvar and its corresponding topology to the tracker
    void AddOrUpdateTopology(const TfToken &primvar, 
                             const VtIntArray &topology);

    /// Remove a primvar from the tracker.
    void RemovePrimvar(const TfToken &primvar);

    /// Remove unused topologies (topologies with no associated primvars), as
    /// we do not want to build stencil tables for them.
    void RemoveUnusedTopologies();

    /// Get the face-varying channel given a primvar name. If the primvar is 
    /// not in the tracker, returns -1.
    int GetChannelFromPrimvar(const TfToken &primvar) const;

    /// Return a vector of all the face-varying topologies.
    std::vector<VtIntArray> GetFvarTopologies() const;

    size_t GetNumTopologies() const {
        return _topologies.size();
    }

private:

    HdTopologyToPrimvarVector _topologies;
};

PXR_NAMESPACE_CLOSE_SCOPE

#endif // PXR_USD_IMAGING_HYDRA_PASSTHROUGH_FVAR_TOPOLOGY_TRACKER_H
