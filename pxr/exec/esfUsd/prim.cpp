//
// Copyright 2025 Pixar
//
// Licensed under the terms set forth in the LICENSE.txt file available at
// https://openusd.org/license.
//
#include "pxr/exec/esfUsd/prim.h"

#include "pxr/exec/esfUsd/attribute.h"

#include <utility>

PXR_NAMESPACE_OPEN_SCOPE

// EsfPrim should not reserve more space than necessary.
static_assert(sizeof(EsfUsd_Prim) == sizeof(EsfPrim));

EsfUsd_Prim::~EsfUsd_Prim() = default;

TfTokenVector
EsfUsd_Prim::_GetAppliedSchemas() const
{
    return _GetWrapped().GetAppliedSchemas();
}

EsfAttribute
EsfUsd_Prim::_GetAttribute(const TfToken &attrName) const
{
    return {
        std::in_place_type<EsfUsd_Attribute>,
        _GetWrapped().GetAttribute(attrName)
    };
}

EsfPrim
EsfUsd_Prim::_GetParent() const
{
    return {std::in_place_type<EsfUsd_Prim>, _GetWrapped().GetParent()};
}

TfType
EsfUsd_Prim::_GetType() const
{
    return _GetWrapped().GetPrimTypeInfo().GetSchemaType();
}

bool
EsfUsd_Prim::IsPseudoRoot() const
{
    return _GetWrapped().IsPseudoRoot();
}

PXR_NAMESPACE_CLOSE_SCOPE