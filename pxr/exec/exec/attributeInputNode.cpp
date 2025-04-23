//
// Copyright 2025 Pixar
//
// Licensed under the terms set forth in the LICENSE.txt file available at
// https://openusd.org/license.
//
#include "pxr/exec/exec/attributeInputNode.h"

#include "pxr/exec/vdf/connectorSpecs.h"
#include "pxr/exec/vdf/context.h"
#include "pxr/exec/vdf/tokens.h"

#include "pxr/base/gf/matrix4d.h"
#include "pxr/base/gf/vec3f.h"
#include "pxr/base/gf/vec3d.h"
#include "pxr/base/vt/types.h"
#include "pxr/base/vt/value.h"
#include "pxr/exec/ef/time.h"
#include "pxr/usd/sdf/valueTypeName.h"
#include "pxr/usd/usd/timeCode.h"

#include <utility>

PXR_NAMESPACE_OPEN_SCOPE

TF_DEFINE_PUBLIC_TOKENS(
    Exec_AttributeInputNodeTokens, EXEC_ATTRIBUTE_INPUT_NODE_TOKENS);

Exec_AttributeInputNode::Exec_AttributeInputNode(
    VdfNetwork *const network,
    EsfAttribute &&attribute,
    EsfJournal *const journal)
    : VdfNode(
        network,
        VdfInputSpecs().ReadConnector(
            TfType::Find<EfTime>(),
            Exec_AttributeInputNodeTokens->time),
        VdfOutputSpecs().Connector(
            attribute->GetValueTypeName(journal)
                .GetScalarType().GetType(),
            TfToken(VdfTokens->out)))
    , _attribute(std::move(attribute))
{
}

Exec_AttributeInputNode::~Exec_AttributeInputNode() = default;

void
Exec_AttributeInputNode::Compute(VdfContext const &context) const
{
    const UsdTimeCode time = context.GetInputValue<EfTime>(
        Exec_AttributeInputNodeTokens->time).GetTimeCode();

    if (VtValue resolvedValue; _attribute->Get(&resolvedValue, time)) {
        // TODO: This will be replaced with more general, registration-based
        // type dispatch
        switch (resolvedValue.GetKnownValueTypeIndex()) {
        case VtGetKnownValueTypeIndex<double>():
            context.SetOutput(resolvedValue.UncheckedGet<double>());
            break;
        case VtGetKnownValueTypeIndex<int>():
            context.SetOutput(resolvedValue.UncheckedGet<int>());
            break;
        case VtGetKnownValueTypeIndex<GfMatrix4d>():
            context.SetOutput(resolvedValue.UncheckedGet<GfMatrix4d>());
            break;
        case VtGetKnownValueTypeIndex<GfVec3f>():
            context.SetOutput(resolvedValue.UncheckedGet<GfVec3f>());
            break;
        case VtGetKnownValueTypeIndex<GfVec3d>():
            context.SetOutput(resolvedValue.UncheckedGet<GfVec3d>());
            break;
        default:
            TF_CODING_ERROR(
                "Support for computing attributes of type '%s' is not yet "
                "implemented.", resolvedValue.GetType().GetTypeName().c_str());
        }
    }
}

PXR_NAMESPACE_CLOSE_SCOPE
