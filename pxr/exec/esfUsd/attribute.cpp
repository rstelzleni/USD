//
// Copyright 2025 Pixar
//
// Licensed under the terms set forth in the LICENSE.txt file available at
// https://openusd.org/license.
//
#include "pxr/exec/esfUsd/attribute.h"

#include "pxr/exec/esf/attribute.h"
#include "pxr/usd/sdf/valueTypeName.h"
#include "pxr/usd/usd/attribute.h"

PXR_NAMESPACE_OPEN_SCOPE

// EsfAttribute should not reserve more space than necessary.
static_assert(sizeof(EsfUsd_Attribute) == sizeof(EsfAttribute));

EsfUsd_Attribute::~EsfUsd_Attribute() = default;

SdfValueTypeName
EsfUsd_Attribute::_GetValueTypeName() const
{
    return _GetWrapped().GetTypeName();
}

bool
EsfUsd_Attribute::_Get(VtValue *value, UsdTimeCode time) const
{
    return _GetWrapped().Get(value, time);
}

PXR_NAMESPACE_CLOSE_SCOPE