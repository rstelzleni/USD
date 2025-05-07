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

class EfTime;
class EsfJournal;

#define EXEC_ATTRIBUTE_INPUT_NODE_TOKENS  \
    (time)

TF_DECLARE_PUBLIC_TOKENS(
    Exec_AttributeInputNodeTokens, EXEC_ATTRIBUTE_INPUT_NODE_TOKENS);

/// Node that computes attribute resolved values.
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

    /// Returns the scene path to the attribute that the input value is sourced
    /// from.
    /// 
    SdfPath GetAttributePath() const {
        return _attribute->GetPath(nullptr);
    }

    /// Returns `true` if the resolved value of the attribute is different on
    /// time \p from and time \p to.
    /// 
    /// \note
    /// This does *not* examine times between \p from and \p to in order to
    /// determine if there is a difference in resolved values on in-between
    /// times.
    ///
    bool IsTimeVarying(const EfTime &from, const EfTime &to) const;

    /// Returns `true` if the attribute might be time-varying, and returns
    /// `false` if the attribute is definitely not time-varying.
    ///
    bool MaybeTimeVarying() const;

    /// VdfNode::Compute() override.
    void Compute(VdfContext const& ctx) const override;

private:
    // Computes dependencies in the output-to-input traversal direction.
    VdfMask::Bits _ComputeInputDependencyMask(
        const VdfMaskedOutput &maskedOutput,
        const VdfConnection &inputConnection) const override;

    // Computes dependencies in the input-to-output traversal direction.
    VdfMask _ComputeOutputDependencyMask(
        const VdfConnection &inputConnection,
        const VdfMask &inputDependencyMask,
        const VdfOutput &output) const override;

private:
    // TODO: Long-term, we will need some kind of handle to the attribute that
    // is stable across namespace edits.
    const EsfAttribute _attribute;
};

PXR_NAMESPACE_CLOSE_SCOPE

#endif
