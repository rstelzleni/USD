//
// Copyright 2025 Pixar
//
// Licensed under the terms set forth in the LICENSE.txt file available at
// https://openusd.org/license.
//
#ifndef PXR_EXEC_EXEC_SYSTEM_CHANGE_MANAGER_H
#define PXR_EXEC_EXEC_SYSTEM_CHANGE_MANAGER_H

/// \file

#include "pxr/pxr.h"

#include "pxr/exec/exec/api.h"
#include "pxr/exec/exec/system.h"

#include "pxr/base/tf/diagnostic.h"
#include "pxr/base/tf/token.h"

PXR_NAMESPACE_OPEN_SCOPE

class SdfPath;

/// Public API to deliver scene changes from ExecSystem derived classes.
///
/// Classes derived from ExecSystem are responsible for notifying ExecSystem
/// when scene changes occur. They do so by constructing an
/// ExecSystem::_ChangeManager from their parent ExecSystem, and invoking
/// methods corresponding to the scene changes.
///
class ExecSystem::_ChangeManager
{
public:
    explicit _ChangeManager(ExecSystem *system) : _system(system) {
        TF_VERIFY(system);
    }

    /// Notifies the ExecSystem that a scene object has been resynced.
    ///
    /// \see UsdNotice::ObjectsChanged::GetResyncedPaths.
    ///
    EXEC_API
    void DidResync(const SdfPath &path) const;

    /// Notifies the ExecSystem that a scene object's fields have changed, but
    /// the object has *not* been resynced.
    ///
    /// \see UsdNotice::ObjectsChanged::GetChangedInfoOnlyPaths.
    /// \see UsdNotice::ObjectsChanged::GetChangedFields.
    ///
    EXEC_API
    void DidChangeInfoOnly(
        const SdfPath &path,
        const TfTokenVector &changedFields) const;

private:
    ExecSystem *const _system;
};

PXR_NAMESPACE_CLOSE_SCOPE

#endif