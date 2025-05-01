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
#include "pxr/exec/exec/types.h"

#include "pxr/exec/esf/stage.h"

#include <tbb/concurrent_vector.h>

#include <memory>
#include <vector>

PXR_NAMESPACE_OPEN_SCOPE

class Exec_Program;
class Exec_RequestImpl;
class ExecValueKey;
template <typename> class TfSpan;
class VdfExecutorErrorLogger;
class VdfExecutorInterface;
class VdfMaskedOutput;
class VdfRequest;
class VdfSchedule;

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
    /// Diagnostic utility class.
    class Diagnostics;

protected:
    /// Construct an exec system for computing values on \p stage.
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

    /// Computes the values in the \p computeRequest using the provided
    /// \p schedule.
    /// 
    EXEC_API
    void _CacheValues(
        const VdfSchedule &schedule,
        const VdfRequest &computeRequest);

    /// Derived systems instantiate this class to deliver scene changes to exec.
    class _ChangeProcessor;

private:
    // Requires access to _Compile
    friend class Exec_RequestImpl;
    std::vector<VdfMaskedOutput> _Compile(TfSpan<const ExecValueKey> valueKeys);

    // Constructs a new instance of the main executor along with its state, and
    // discards the previous instance if any.
    // 
    EXEC_API
    void _CreateExecutorState();

    // Returns a pointer to the main executor.
    EXEC_API
    VdfExecutorInterface *_GetMainExecutor();

    // Discards all internal state, and constructs new internal data structures
    // leaving the system in the same state as if it was newly constructed.
    // 
    EXEC_API
    void _InvalidateAll();

    // Notifies the system of authored value invalidation.
    EXEC_API
    void _InvalidateAuthoredValues(
        TfSpan<ExecInvalidAuthoredValue> invalidProperties);

    // Reports any executor errors raised during a round of evaluation.
    EXEC_API
    void _ReportExecutorErrors(const VdfExecutorErrorLogger &errorLogger) const;

private:
    EsfStage _stage;

    std::unique_ptr<Exec_Program> _program;

    class _ExecutorState;
    std::unique_ptr<_ExecutorState> _executorState;

    tbb::concurrent_vector<std::shared_ptr<Exec_RequestImpl>> _requests;
};

PXR_NAMESPACE_CLOSE_SCOPE

#endif
