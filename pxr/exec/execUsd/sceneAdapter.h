//
// Copyright 2025 Pixar
//
// Licensed under the terms set forth in the LICENSE.txt file available at
// https://openusd.org/license.
//
#ifndef PXR_EXEC_EXEC_USD_SCENE_ADAPTER_H
#define PXR_EXEC_EXEC_USD_SCENE_ADAPTER_H

/// \file

#include "pxr/pxr.h"

#include "pxr/exec/execUsd/api.h"

#include "pxr/exec/esf/attribute.h"
#include "pxr/exec/esf/object.h"
#include "pxr/exec/esf/prim.h"
#include "pxr/exec/esf/property.h"
#include "pxr/exec/esf/stage.h"
#include "pxr/usd/usd/attribute.h"
#include "pxr/usd/usd/common.h"
#include "pxr/usd/usd/object.h"
#include "pxr/usd/usd/prim.h"
#include "pxr/usd/usd/property.h"

PXR_NAMESPACE_OPEN_SCOPE

/// Family of static factory methods that produce abstract scene objects from
/// USD scene objects.
///
/// The underlying implementations of the scene object interfaces are not
/// exported by ExecUsd. Clients can only obtain abstract scene objects by using
/// this class.
///
struct ExecUsdSceneAdapter
{
    static EXECUSD_API
    EsfStage AdaptStage(const UsdStageConstRefPtr &stage);

    static EXECUSD_API
    EsfStage AdaptStage(UsdStageConstRefPtr &&stage);

    static EXECUSD_API
    EsfObject AdaptObject(const UsdObject &object);

    static EXECUSD_API
    EsfObject AdaptObject(UsdObject &&object);

    static EXECUSD_API
    EsfPrim AdaptPrim(const UsdPrim &prim);

    static EXECUSD_API
    EsfPrim AdaptPrim(UsdPrim &&prim);

    static EXECUSD_API
    EsfProperty AdaptProperty(const UsdProperty &property);

    static EXECUSD_API
    EsfProperty AdaptProperty(UsdProperty &&property);

    static EXECUSD_API
    EsfAttribute AdaptAttribute(const UsdAttribute &attribute);

    static EXECUSD_API
    EsfAttribute AdaptAttribute(UsdAttribute &&attribute);
};

PXR_NAMESPACE_CLOSE_SCOPE

#endif