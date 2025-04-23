//
// Copyright 2025 Pixar
//
// Licensed under the terms set forth in the LICENSE.txt file available at
// https://openusd.org/license.
//
#ifndef PXR_EXEC_EXEC_REQUEST_IMPL_H
#define PXR_EXEC_EXEC_REQUEST_IMPL_H

#include "pxr/pxr.h"

#include "pxr/exec/exec/api.h"

#include <memory>
#include <vector>

PXR_NAMESPACE_OPEN_SCOPE

class ExecSystem;
class ExecValueKey;
template <typename> class TfSpan;
class VdfMaskedOutput;
class VdfRequest;
class VdfSchedule;

/// Contains data structures necessary to implement exec requests that are
/// independent of scene description.
///
/// Concrete implementations inherit from Exec_RequestImpl to implement any
/// functionality that is specific to the scene description system.
///
class Exec_RequestImpl
{
protected:
    EXEC_API
    Exec_RequestImpl();

    Exec_RequestImpl(const Exec_RequestImpl&) = delete;
    Exec_RequestImpl& operator=(const Exec_RequestImpl&) = delete;

    EXEC_API
    ~Exec_RequestImpl();

    /// Compiles outputs for the value keys in the request.
    EXEC_API
    void _Compile(ExecSystem *system, TfSpan<const ExecValueKey> valueKeys);

    /// Builds the schedule for the request.
    EXEC_API
    void _Schedule();

private:
    std::vector<VdfMaskedOutput> _leafOutputs;

    std::unique_ptr<VdfRequest> _computeRequest;
    std::unique_ptr<VdfSchedule> _schedule;
};

PXR_NAMESPACE_CLOSE_SCOPE

#endif
