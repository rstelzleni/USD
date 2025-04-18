//
// Copyright 2025 Pixar
//
// Licensed under the terms set forth in the LICENSE.txt file available at
// https://openusd.org/license.
//
#include "pxr/exec/execUsd/request.h"

PXR_NAMESPACE_OPEN_SCOPE

ExecUsdRequest::~ExecUsdRequest() = default;

bool
ExecUsdRequest::IsValid() const
{
    return !_impl.expired();
}

PXR_NAMESPACE_CLOSE_SCOPE
