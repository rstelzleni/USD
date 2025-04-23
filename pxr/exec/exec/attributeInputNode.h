//
// Copyright 2025 Pixar
//
// Licensed under the terms set forth in the LICENSE.txt file available at
// https://openusd.org/license.
//
#ifndef PXR_EXEC_EXEC_ATTRIBUTE_INPUT_NODE_H
#define PXR_EXEC_EXEC_ATTRIBUTE_INPUT_NODE_H

#include "pxr/pxr.h"

#include "pxr/exec/esf/attribute.h"
#include "pxr/exec/vdf/node.h"

#include "pxr/base/tf/staticTokens.h"

PXR_NAMESPACE_OPEN_SCOPE

class EsfJournal;

#define EXEC_ATTRIBUTE_INPUT_NODE_TOKENS  \
    (time)

TF_DECLARE_PUBLIC_TOKENS(
    Exec_AttributeInputNodeTokens, EXEC_ATTRIBUTE_INPUT_NODE_TOKENS);

/// Node that computes attribute resolved values.
///
class Exec_AttributeInputNode final : public VdfNode
{
public:
    /// Create a node that provides the resolved value of \p attribute at the
    /// current evaluation time.
    ///
    Exec_AttributeInputNode(
        VdfNetwork *network,
        EsfAttribute &&attribute,
        EsfJournal *journal);

    ~Exec_AttributeInputNode() override;

    void Compute(VdfContext const& ctx) const override;

private:
    // TODO: Long-term, we will need some kind of handle to the attribute that is
    // stable across namespace edits.
    const EsfAttribute _attribute;
};

PXR_NAMESPACE_CLOSE_SCOPE

#endif
