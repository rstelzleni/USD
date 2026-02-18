#include "fvarTopologyTracker.h"

PXR_NAMESPACE_OPEN_SCOPE

void 
HydraPassthroughFvarTopologyTracker::AddOrUpdateTopology(
        const TfToken &primvar, 
        const VtIntArray &topology) {
    for (size_t i = 0; i < _topologies.size(); ++i) {
        // Found existing topology
        if (_topologies[i].first == topology) {

            if (std::find(_topologies[i].second.begin(),
                          _topologies[i].second.end(),
                          primvar) == _topologies[i].second.end()) {
                // Topology does not have that primvar assigned
                RemovePrimvar(primvar);
                _topologies[i].second.push_back(primvar);
            } 
            return;
        } 
    }

    // Found new topology
    RemovePrimvar(primvar);
    _topologies.push_back(
            std::pair<VtIntArray, std::vector<TfToken>>(
                topology, {primvar}));
}

void
HydraPassthroughFvarTopologyTracker::RemovePrimvar(const TfToken &primvar) {
    for (size_t i = 0; i < _topologies.size(); ++i) {
        _topologies[i].second.erase(std::find(
                    _topologies[i].second.begin(),
                    _topologies[i].second.end(),
                    primvar), _topologies[i].second.end());

    }
}

void
HydraPassthroughFvarTopologyTracker::RemoveUnusedTopologies() {
    _topologies.erase(
            std::remove_if(
                _topologies.begin(), _topologies.end(), 
                [](const auto& x) {
                    return x.second.empty();
                }),
            _topologies.end());
}

int
HydraPassthroughFvarTopologyTracker::GetChannelFromPrimvar(const TfToken &primvar) const {
    for (size_t i = 0; i < _topologies.size(); ++i) {
        if (std::find(_topologies[i].second.begin(),
                      _topologies[i].second.end(),
                      primvar) != 
                _topologies[i].second.end()) {
            return i;
        }
    }
    return -1;
}

// Return a vector of all the face-varying topologies.
std::vector<VtIntArray>
HydraPassthroughFvarTopologyTracker::GetFvarTopologies() const {
    std::vector<VtIntArray> fvarTopologies;
    for (const auto& it : _topologies) {
        fvarTopologies.push_back(it.first);
    }
    return fvarTopologies;
}

PXR_NAMESPACE_CLOSE_SCOPE
