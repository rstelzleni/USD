//
// Copyright 2025 Pixar
//
// Licensed under the terms set forth in the LICENSE.txt file available at
// https://openusd.org/license.
//
#include "pxr/exec/vdf/isolatedSubnetwork.h"

#include "pxr/exec/vdf/connection.h"

#include "pxr/base/trace/trace.h"

#include <stack>

PXR_NAMESPACE_OPEN_SCOPE

VdfIsolatedSubnetworkRefPtr
VdfIsolatedSubnetwork::IsolateBranch(
    VdfConnection *const connection,
    VdfNetwork::EditFilter *const filter)
{
    if (!connection) {
        TF_CODING_ERROR("Null connection");
        return nullptr;
    }

    VdfIsolatedSubnetwork *const isolated = new VdfIsolatedSubnetwork(
        &connection->GetTargetNode().GetNetwork());

    if (!isolated->AddIsolatedBranch(connection, filter)) {
        delete isolated;
        return nullptr;
    }

    isolated->RemoveIsolatedObjectsFromNetwork();

    return TfCreateRefPtr(isolated);
}

VdfIsolatedSubnetworkRefPtr
VdfIsolatedSubnetwork::IsolateBranch(
    VdfNode *const node,
    VdfNetwork::EditFilter *const filter)
{
    if (!node) {
        TF_CODING_ERROR("Null node");
        return nullptr;
    }
    if (node->HasOutputConnections()) {
        TF_CODING_ERROR("Root node has output connections.");
        return nullptr;
    }

    // If we can't delete the initial node, we bail early.
    if (filter && !filter->CanDelete(node)) {
        return nullptr;
    }

    VdfIsolatedSubnetwork *const isolated =
        new VdfIsolatedSubnetwork(&node->GetNetwork());

    if (!isolated->AddIsolatedBranch(node, filter)) {
        delete isolated;
        return nullptr;
    }
    
    isolated->RemoveIsolatedObjectsFromNetwork();

    return TfCreateRefPtr(isolated);
}

VdfIsolatedSubnetworkRefPtr
VdfIsolatedSubnetwork::New(VdfNetwork *const network)
{
    if (!network) {
        TF_CODING_ERROR("Null network");
        return nullptr;
    }

    return TfCreateRefPtr(new VdfIsolatedSubnetwork(network));
}

bool
VdfIsolatedSubnetwork::AddIsolatedBranch(
    VdfConnection *const connection,
    VdfNetwork::EditFilter *const filter)
{
    if (!connection) {
        TF_CODING_ERROR("Null connection");
        return false;
    }
    if (&connection->GetTargetNode().GetNetwork() != _network) {
        TF_CODING_ERROR(
            "Attempt to call AddIsolatedBranch with a connection from a "
            "different network.");
        return false;
    }
    if (_removedIsolatedObjects) {
        TF_CODING_ERROR(
            "Attempt to call AddIsolatedBranch after calling "
            "RemoveIsolatedObjectsFromNetwork");
        return false;
    }

    // Collect all nodes/connections that are reachable from the input side
    // of the connection.
    _TraverseBranch(connection, filter);

    return true;
}

bool
VdfIsolatedSubnetwork::AddIsolatedBranch(
    VdfNode *const node,
    VdfNetwork::EditFilter *const filter)
{
    if (!node) {
        TF_CODING_ERROR("Null node");
        return false;
    }
    if (&node->GetNetwork() != _network) {
        TF_CODING_ERROR(
            "Attempt to call AddIsolatedBranch with a node from a different "
            "network.");
        return false;
    }
    if (_removedIsolatedObjects) {
        TF_CODING_ERROR(
            "Attempt to call AddIsolatedBranch after calling "
            "RemoveIsolatedObjectsFromNetwork");
        return false;
    }

    // If we can't delete the initial node, we bail early.
    if (node->HasOutputConnections() ||
        (filter && !filter->CanDelete(node))) {
        return false;
    }

    // Collect all nodes/connections reachable from node.
    // Traverse up all input connections.
    for (VdfConnection *const c :  node->GetInputConnections()) {
        _TraverseBranch(c, filter);
    }
    
    _nodes.insert(node);

    return true;
}
    
VdfIsolatedSubnetwork::VdfIsolatedSubnetwork(VdfNetwork *network)
: _network(network)
{
    TF_VERIFY(_network);
}

VdfIsolatedSubnetwork::~VdfIsolatedSubnetwork()
{
    TRACE_FUNCTION();

    // Make sure isolated objects get removed from the network before we delete
    // them.
    if (!_removedIsolatedObjects) {
        RemoveIsolatedObjectsFromNetwork();
    }

    for (VdfConnection *const c : _connections) {
        _network->_DeleteConnection(c);
    }

    for (VdfNode *const n : _nodes) {
        _network->_DeleteNode(n);
    }
}

bool
VdfIsolatedSubnetwork::_CanTraverse(
    VdfConnection *connection,
    VdfNetwork::EditFilter *filter,
    const VdfConnectionSet &visitedConnections)
{
    VdfNode &sourceNode = connection->GetSourceNode();

    // Can we delete the source node of that connection? If so,
    // recurse, else ignore this connection.
    if (filter && !filter->CanDelete(&sourceNode)) {
        return false;
    }

    // If there is more than one output connection (ie. an additional one
    // besides the one we use to discover this node), stop traversing since
    // other nodes are using part of the network above this point.
    //
    // Since we don't delete connection right away (in order to get correct 
    // paths), we need to see what we've seen before in order to determine if
    // there is an additional output.
    for (VdfConnection *const connection : sourceNode.GetOutputConnections()) {
        if (visitedConnections.count(connection) == 0) {
            return false;
        }
    }

    return true;
}

void
VdfIsolatedSubnetwork::_TraverseBranch(
    VdfConnection           *connection,
    VdfNetwork::EditFilter  *filter)
{
    std::stack<VdfConnection*> stack;
    stack.push(connection);

    while (!stack.empty()) {
        VdfConnection *const currentConnection = stack.top();
        stack.pop();

        // Mark this connection as visited.
        _connections.insert(currentConnection);
    
        // We can't traverse this connection, therefore we stop.
        if (!_CanTraverse(currentConnection, filter, _connections)) {
            continue;
        }
    
        VdfNode &sourceNode = currentConnection->GetSourceNode();
    
        const bool inserted = _nodes.insert(&sourceNode).second;
    
        // We only traverse if the object wasn't visited before. This can 
        // happen if different inputs of the same node are connected to source 
        // outputs of the same node.
        if (inserted) {
            // Push the connections onto the stack in reverse order, so that
            // on the next iteration, the first connection lives on top of the
            // stack and gets picked up first.
            const VdfConnectionVector inputConnections =
                sourceNode.GetInputConnections();
            auto rit = inputConnections.crbegin();
            const auto rend = inputConnections.crend();
            for (; rit != rend; ++rit) {
                stack.push(*rit);
            }
        }
    }
}

void
VdfIsolatedSubnetwork::RemoveIsolatedObjectsFromNetwork()
{
    TRACE_FUNCTION();

    // First, remove all connections. This happens before any nodes are removed
    // to match the order in which the VdfNetwork sends out deletion notices.
    for (VdfConnection *const c : _connections) {
        _network->_RemoveConnection(c);
    }

    // Remove all nodes from the network so that the network is in a consistent
    // state. Note that the nodes are not deleted.
    for (VdfNode *const n : _nodes) {
        _network->_RemoveNode(n);
    }

    _removedIsolatedObjects = true;
}

PXR_NAMESPACE_CLOSE_SCOPE
