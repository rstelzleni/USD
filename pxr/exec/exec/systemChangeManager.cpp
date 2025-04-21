//
// Copyright 2025 Pixar
//
// Licensed under the terms set forth in the LICENSE.txt file available at
// https://openusd.org/license.
//
#include "pxr/exec/exec/systemChangeManager.h"

#include "pxr/usd/sdf/path.h"

PXR_NAMESPACE_OPEN_SCOPE

void ExecSystem::_ChangeManager::DidResync(const SdfPath &path) const
{
    // TODO
}

void ExecSystem::_ChangeManager::DidChangeInfoOnly(
    const SdfPath &path,
    const TfTokenVector &changedFields) const
{
    // TODO
}

PXR_NAMESPACE_CLOSE_SCOPE
