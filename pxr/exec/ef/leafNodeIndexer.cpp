//
// Copyright 2025 Pixar
//
// Licensed under the terms set forth in the LICENSE.txt file available at
// https://openusd.org/license.
//
#include "pxr/exec/ef/leafNodeIndexer.h"

#include "pxr/exec/ef/leafNode.h"

#include "pxr/base/arch/hints.h"
#include "pxr/base/trace/trace.h"

PXR_NAMESPACE_OPEN_SCOPE

const Ef_LeafNodeIndexer::Index Ef_LeafNodeIndexer::InvalidIndex(-1);

void
Ef_LeafNodeIndexer::Invalidate()
{
    TRACE_FUNCTION();

    _indices.clear();
    _nodes.clear();
    _freeList.clear();
}

void
Ef_LeafNodeIndexer::DidDisconnect(const VdfConnection &connection)
{
    // Bail out if the connection does not target a leaf node.
    const VdfNode &leafNode = connection.GetTargetNode();
    if (!EfLeafNode::IsALeafNode(leafNode)) {
        return;
    }

    TfAutoMallocTag2 tag("Ef", "Ef_LeafNodeIndexer::DidDisconnect");

    // Find the index of the targeted node.
    const VdfIndex nodeIndex = VdfNode::GetIndexFromId(leafNode.GetId());
    TF_DEV_AXIOM(_indices.size() > nodeIndex);

    // Find the leaf node index using the node index.
    Index *index = &_indices[nodeIndex];
    TF_DEV_AXIOM(*index != InvalidIndex);

    // If this is the last assigned index, pop the entry from the leaf node
    // data vector. Otherwise, push the index on the free list.
    if ((*index + 1) == _nodes.size()) {
        _nodes.pop_back();
    } else {
        _freeList.push_back(*index);
    }

    // The index is now unassigned.
    *index = InvalidIndex;
}

void
Ef_LeafNodeIndexer::DidConnect(const VdfConnection &connection)
{
    // Bail out if the connection does not target a leaf node.
    const VdfNode &leafNode = connection.GetTargetNode();
    if (!EfLeafNode::IsALeafNode(leafNode)) {
        return;
    }

    TfAutoMallocTag2 tag("Ef", "Ef_LeafNodeIndexer::DidConnect");
    
    // Find the index of the targeted node, and make sure that the vector
    // storing the leaf node indices is appropriately sized.
    const VdfIndex nodeIndex = VdfNode::GetIndexFromId(leafNode.GetId());
    if (ARCH_UNLIKELY(nodeIndex >= _indices.size())) {
        const size_t newSize = nodeIndex + 1;
        _indices.resize(newSize + (newSize / 2), InvalidIndex);
    }

    // Get the output and mask of the connected source output.
    const VdfOutput *output = &connection.GetSourceOutput();
    const VdfMask *mask = &connection.GetMask();

    // Get a pointer to the leaf node index. It should be unassigned at this
    // point.
    Index *index = &_indices[nodeIndex];
    TF_DEV_AXIOM(*index == InvalidIndex);

    // If the free list is empty, let's append the new leaf node data entry to
    // the vector.
    if (_freeList.empty()) {
        *index = _nodes.size();
        _nodes.push_back({&leafNode, output, mask});
    }

    // If there is an entry on the free list, let's re-use that one instead.
    else {
        *index = _freeList.back();
        _freeList.pop_back();
        _nodes[*index] = _LeafNode{&leafNode, output, mask};
    }
}

PXR_NAMESPACE_CLOSE_SCOPE
