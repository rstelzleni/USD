//
// Copyright 2025 Pixar
//
// Licensed under the terms set forth in the LICENSE.txt file available at
// https://openusd.org/license.
//
#ifndef PXR_EXEC_EXEC_USD_STAGE_H
#define PXR_EXEC_EXEC_USD_STAGE_H

/// \file

#include "pxr/pxr.h"

#include "pxr/exec/esf/stage.h"
#include "pxr/usd/usd/common.h"

PXR_NAMESPACE_OPEN_SCOPE

/// Implementation of EsfStageInterface that wraps a UsdStageConstRefPtr.
class ExecUsd_Stage : public EsfStageInterface
{
public:
    ~ExecUsd_Stage() override;

    /// Copies the provided \p stage pointer into this instance.
    ///
    /// \p stage must not be a null pointer.
    ///
    ExecUsd_Stage(const UsdStageConstRefPtr &stage);

    /// Moves the provided \p stage pointer into this instance.
    ///
    /// \p stage must not be a null pointer.
    ///
    ExecUsd_Stage(UsdStageConstRefPtr &&stage);

private:
    // EsfStageInterface implementation.
    EsfAttribute _GetAttributeAtPath(const SdfPath &path) const final;
    EsfObject _GetObjectAtPath(const SdfPath &path) const final;
    EsfPrim _GetPrimAtPath(const SdfPath &path) const final;
    EsfProperty _GetPropertyAtPath(const SdfPath &path) const final;

    UsdStageConstRefPtr _stage;
};

PXR_NAMESPACE_CLOSE_SCOPE

#endif