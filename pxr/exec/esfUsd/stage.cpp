//
// Copyright 2025 Pixar
//
// Licensed under the terms set forth in the LICENSE.txt file available at
// https://openusd.org/license.
//
#include "pxr/exec/esfUsd/stage.h"

#include "pxr/exec/esfUsd/attribute.h"
#include "pxr/exec/esfUsd/object.h"
#include "pxr/exec/esfUsd/prim.h"
#include "pxr/exec/esfUsd/property.h"

#include "pxr/base/tf/diagnosticLite.h"

#include <utility>

PXR_NAMESPACE_OPEN_SCOPE

// EsfStage should not reserve more space than necessary.
static_assert(sizeof(EsfUsd_Stage) == sizeof(EsfStage));

EsfUsd_Stage::~EsfUsd_Stage() = default;

EsfUsd_Stage::EsfUsd_Stage(const UsdStageConstRefPtr &stage)
    : _stage(stage)
{
    TF_VERIFY(_stage);
}

EsfUsd_Stage::EsfUsd_Stage(UsdStageConstRefPtr &&stage)
    : _stage(std::move(stage))
{
    TF_VERIFY(_stage);
}

EsfAttribute
EsfUsd_Stage::_GetAttributeAtPath(const SdfPath &path) const
{
    return {
        std::in_place_type<EsfUsd_Attribute>,
        _stage->GetAttributeAtPath(path)
    };
}

EsfObject
EsfUsd_Stage::_GetObjectAtPath(const SdfPath &path) const
{
    return {std::in_place_type<EsfUsd_Object>, _stage->GetObjectAtPath(path)};
}

EsfPrim
EsfUsd_Stage::_GetPrimAtPath(const SdfPath &path) const
{
    return {std::in_place_type<EsfUsd_Prim>, _stage->GetPrimAtPath(path)};
}

EsfProperty
EsfUsd_Stage::_GetPropertyAtPath(const SdfPath &path) const
{
    return {
        std::in_place_type<EsfUsd_Property>,
        _stage->GetPropertyAtPath(path)
    };
}

PXR_NAMESPACE_CLOSE_SCOPE
