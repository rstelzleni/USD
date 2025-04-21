//
// Copyright 2025 Pixar
//
// Licensed under the terms set forth in the LICENSE.txt file available at
// https://openusd.org/license.
//
#ifndef PXR_EXEC_EXEC_SYSTEM_H
#define PXR_EXEC_EXEC_SYSTEM_H

#include "pxr/pxr.h"

#include "pxr/exec/exec/api.h"

#include "pxr/exec/esf/stage.h"

#include <tbb/concurrent_vector.h>

#include <memory>
#include <vector>

PXR_NAMESPACE_OPEN_SCOPE

class Exec_Program;
class Exec_RequestImpl;
class ExecValueKey;

template <typename> class TfSpan;
class VdfExecutorInterface;
class VdfMaskedOutput;

/// Base implementation of a system to procedurally compute values based on
/// scene description and computation definitions.
///
/// ExecSystem owns all the structures necessary to compile, schedule and
/// evaluate requested computation values.  Derived classes are responsible
/// for interfacing with the underlying scene description.
///
class ExecSystem
{
public:
    EXEC_API
    void GraphNetwork(const char *filename) const;

protected:
    /// Construct an exec system for computing values on \p stage.
    ///
    EXEC_API
    explicit ExecSystem(EsfStage &&stage);

    ExecSystem(const ExecSystem &) = delete;
    ExecSystem& operator=(const ExecSystem &) = delete;

    EXEC_API
    ~ExecSystem();

    /// Transfer ownership of a newly-created request impl to the system.
    ///
    /// The system is responsible for managing the lifetime of the impl in
    /// response to scene changes that would affect it.
    ///
    EXEC_API
    void _InsertRequest(std::shared_ptr<Exec_RequestImpl> &&impl);

    /// Derived systems instantiate this class to deliver scene changes to exec.
    class _ChangeManager;

private:
    // Requires access to _Compile
    friend class Exec_RequestImpl;
    std::vector<VdfMaskedOutput> _Compile(TfSpan<const ExecValueKey> valueKeys);

private:
    EsfStage _stage;

    std::unique_ptr<Exec_Program> _program;

    std::unique_ptr<VdfExecutorInterface> _executor;
    tbb::concurrent_vector<std::shared_ptr<Exec_RequestImpl>> _requests;
};

PXR_NAMESPACE_CLOSE_SCOPE

#endif
