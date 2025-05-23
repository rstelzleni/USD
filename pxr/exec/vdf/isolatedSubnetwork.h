//
// Copyright 2025 Pixar
//
// Licensed under the terms set forth in the LICENSE.txt file available at
// https://openusd.org/license.
//
#ifndef PXR_EXEC_VDF_ISOLATED_SUBNETWORK_H
#define PXR_EXEC_VDF_ISOLATED_SUBNETWORK_H

/// \file

#include "pxr/pxr.h"

#include "pxr/exec/vdf/api.h"
#include "pxr/exec/vdf/network.h"
#include "pxr/exec/vdf/types.h"

#include "pxr/base/tf/hashset.h"

PXR_NAMESPACE_OPEN_SCOPE

class VdfNode;

////////////////////////////////////////////////////////////////////////////////
///
/// A VdfIsolatedSubnetwork builds a collection of VdfNode%s and VdfConnection%s
/// that are disconnected from the owning network.
///
/// Building an isolated subnetwork proceeds in three phases:
/// 
/// 1. A traversal starts from one or more nodes or connections and proceeds in
///    the input direction, and identifies all reachable objects that are not
///    otherwise connected to the network. I.e., the traversal stops at nodes
///    that have output connections that are not part of the isolated
///    subnetwork.
///
/// 2. Isolated objects are removed from the network, which causes WillDelete
///    notices to be sent, out even though objects have not yet been deleted.
///    This process transfers ownership of network objects from the VdfNetwork
///    to the VdfIsolatedSubnetwork.
///
/// 3. The objects are deleted when the VdfIsolatedSubnetwork is deleted.
///
class VdfIsolatedSubnetwork
{
public:
    VdfIsolatedSubnetwork(const VdfIsolatedSubnetwork &) = delete;
    VdfIsolatedSubnetwork &operator=(const VdfIsolatedSubnetwork &) = delete;

    VDF_API
    ~VdfIsolatedSubnetwork();

    /// A set of nodes used during deletion of sub-networks.
    using NodeSet = TfHashSet<VdfNode*, TfHash>;

    /// Isolates all nodes and connections reachable via input connections from
    /// \p connection that are not connected via additional output connections
    /// to other parts of the network.
    ///
    /// Note that \p connection is added to the set of isolated objects.
    ///
    /// Optional \p filter object can be used to prune the traversal.
    ///
    /// Removes the isolated objects from the network and returns a unique
    /// pointer to the isolated network object that holds onto the isolated
    /// nodes and connections. When the isolated network object is deleted, the
    /// isolated nodes and connections are deleted.
    ///
    VDF_API static
    std::unique_ptr<VdfIsolatedSubnetwork> IsolateBranch(
        VdfConnection *connection,
        VdfNetwork::EditFilter *filter);

    /// Isolates all nodes and connections reachable via input connections from
    /// \p node that are not connected via additional output connections to
    /// other parts of the network.
    ///
    /// Optional \p filter object can be used to prune the traversal.
    ///
    /// Removes the isolated objects from the network and returns a strong
    /// reference to the isolated network object that holds onto the isolated
    /// nodes and connections. When the isolated network object is deleted, the
    /// isolated nodes and connections are deleted.
    ///
    /// \note
    /// An error is emitted if \p node has output connections.
    ///
    VDF_API static
    std::unique_ptr<VdfIsolatedSubnetwork> IsolateBranch(
        VdfNode *node,
        VdfNetwork::EditFilter *filter);

    /// Creates an empty isolated subnetwork.
    ///
    /// The subnetwork can be populated via calls to the AddIsolatedBranch
    /// methods.
    ///
    VDF_API static
    std::unique_ptr<VdfIsolatedSubnetwork> New(VdfNetwork *network);

    /// Isolates all nodes and connections reachable via input connections from
    /// \p connection that are not connected via additional output connections
    /// to other parts of the network.
    ///
    /// Note that \p connection is added to the set of isolated objects.
    ///
    /// Optional \p filter object can be used to prune the traversal.
    /// 
    /// \note
    /// Isolated objects are not immediately removed from the network. See
    /// RemoveIsolatedObjectsFromNetwork.
    ///
    VDF_API
    bool AddIsolatedBranch(
        VdfConnection *connection,
        VdfNetwork::EditFilter *filter);

    /// Isolates all nodes and connections reachable via input connections from
    /// \p node that are not connected via additional output connections to
    /// other parts of the network.
    ///
    /// Optional \p filter object can be used to prune the traversal.
    ///
    /// \note
    /// If \p node has output connections or \p filter returns `false` for \p
    /// node, no objects are added to the isolated subnetwork and `false` is
    /// returned.
    /// 
    /// \note
    /// Isolated objects are not immediately removed from the network. See
    /// RemoveIsolatedObjectsFromNetwork.
    ///
    VDF_API
    bool AddIsolatedBranch(
        VdfNode *node,
        VdfNetwork::EditFilter *filter);

    /// Removes all isolated objects from the network.
    ///
    /// This method is called upon destruction, if it isn't called before then.
    ///
    VDF_API
    void RemoveIsolatedObjectsFromNetwork();

    /// Returns the set of isolated nodes.
    const NodeSet &GetIsolatedNodes() const {
        return _nodes;
    }

    /// Returns the set of isolated nodes.
    const VdfConnectionSet &GetIsolatedConnections() const {
        return _connections;
    }

private:
    VdfIsolatedSubnetwork(VdfNetwork *network);

    // Helper that checks if we can traverse a connection.  
    static bool _CanTraverse(
        VdfConnection          *connection,
        VdfNetwork::EditFilter *filter,
        const VdfConnectionSet &visitedConnections);

    // Helper that traverses a branch.
    void _TraverseBranch(
        VdfConnection          *connection,
        VdfNetwork::EditFilter *filter);

private:

    // The network
    VdfNetwork *_network;

    // The set of isolated nodes.
    NodeSet _nodes;
    
    // The set of isolated connections.
    VdfConnectionSet _connections;

    // Flag that indicates whether or not RemoveIsolatedObjectsFromNetwork has
    // been called.
    bool _removedIsolatedObjects = false;
};

PXR_NAMESPACE_CLOSE_SCOPE

#endif
