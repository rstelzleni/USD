//
// Copyright 2025 Pixar
//
// Licensed under the terms set forth in the LICENSE.txt file available at
// https://openusd.org/license.
//
#ifndef PXR_EXEC_EF_LEAF_NODE_H
#define PXR_EXEC_EF_LEAF_NODE_H

/// \file

#include "pxr/pxr.h"

#include "pxr/exec/ef/api.h"

#include "pxr/base/tf/staticTokens.h"
#include "pxr/exec/vdf/node.h"
#include "pxr/exec/vdf/connection.h"

PXR_NAMESPACE_OPEN_SCOPE

#define EF_LEAF_TOKENS \
    (in)

TF_DECLARE_PUBLIC_TOKENS(EfLeafTokens, EF_API, EF_LEAF_TOKENS);

///////////////////////////////////////////////////////////////////////////////
///
/// \class EfLeafNode
///
/// \brief A terminal node, which is never executed.
///
/// Leaf nodes are used for creating terminal nodes that are visited during
/// invalidation.  Invalidation callbacks on these leaf nodes causes
/// downstream invalidation notification to be sent.
///
class EF_API_TYPE EfLeafNode final : public VdfNode
{
public:
    /// Returns \c true if the given node is an EfLeafNode. This method is
    /// an accelerated alternative to IsA<EfLeafNode>() or dynamic_cast.
    ///
    static bool IsALeafNode(const VdfNode &node) {
        return node.GetNumOutputs() == 0 && node.IsA<EfLeafNode>();
    }

    /// Returns the single output the leaf node sources its value from. Returns
    /// \c nullptr if the leaf node is not connected.
    ///
    static const VdfOutput *GetSourceOutput(const VdfNode &node) {
        const VdfInput *input = node.GetInputsIterator().begin()->second;
        return input ? &(*input)[0].GetSourceOutput() : nullptr;
    }

    EF_API
    EfLeafNode(VdfNetwork *network, TfType inputType);

private:

    /// Should never be called.
    virtual void Compute(const VdfContext &context) const override;

    // Only a network is allowed to delete nodes.
    virtual ~EfLeafNode();
};

PXR_NAMESPACE_CLOSE_SCOPE

#endif