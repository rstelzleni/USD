//
// Copyright 2025 Pixar
//
// Licensed under the terms set forth in the LICENSE.txt file available at
// https://openusd.org/license.
//
#include "pxr/exec/exec/system.h"

#include "pxr/exec/exec/authoredValueInvalidationResult.h"
#include "pxr/exec/exec/compiler.h"
#include "pxr/exec/exec/program.h"
#include "pxr/exec/exec/requestImpl.h"
#include "pxr/exec/exec/timeChangeInvalidationResult.h"

#include "pxr/exec/ef/executor.h"
#include "pxr/exec/ef/time.h"
#include "pxr/exec/ef/timeInterval.h"
#include "pxr/exec/vdf/dataManagerVector.h"
#include "pxr/exec/vdf/executorErrorLogger.h"
#include "pxr/exec/vdf/parallelDataManagerVector.h"
#include "pxr/exec/vdf/parallelExecutorEngine.h"
#include "pxr/exec/vdf/parallelSpeculationExecutorEngine.h"
#include "pxr/exec/vdf/pullBasedExecutorEngine.h"

#include "pxr/base/tf/span.h"
#include "pxr/base/trace/trace.h"
#include "pxr/base/work/loops.h"

#include <utility>

PXR_NAMESPACE_OPEN_SCOPE

// Encapsulates the state of the main executor and performs topological state
// invalidation before handing out access to the executor.
// 
class ExecSystem::_ExecutorState 
{
public:
    _ExecutorState();

    VdfExecutorInterface *GetExecutor(size_t networkVersion);

private:
    std::unique_ptr<VdfExecutorInterface> _executor = nullptr;
    size_t _lastNetworkVersion = 0;
};

ExecSystem::_ExecutorState::_ExecutorState()
{
    if (VdfIsParallelEvaluationEnabled()) {
        _executor = std::make_unique<
            EfExecutor<
                VdfParallelExecutorEngine,
                VdfParallelDataManagerVector>>();
    } else {
        _executor = std::make_unique<
            EfExecutor<
                VdfPullBasedExecutorEngine,
                VdfDataManagerVector<
                    VdfDataManagerDeallocationMode::Background>>>();
    }
}

VdfExecutorInterface *
ExecSystem::_ExecutorState::GetExecutor(const size_t networkVersion)
{
    // If the last recorded network version is different from the network
    // version the executor was retrieved with, we need to make sure to
    // invalidate the executor's topological state before handing it out.
    if (networkVersion != _lastNetworkVersion) {
        _executor->InvalidateTopologicalState();
        _lastNetworkVersion = networkVersion;
    }

    return _executor.get();
}

ExecSystem::ExecSystem(EsfStage &&stage)
    : _stage(std::move(stage))
    , _program(std::make_unique<Exec_Program>())
    , _executorState(std::make_unique<ExecSystem::_ExecutorState>())
{
    _ChangeTime(EfTime());
}

ExecSystem::~ExecSystem() = default;

void
ExecSystem::_InsertRequest(std::shared_ptr<Exec_RequestImpl> &&impl)
{
    _requests.push_back(std::move(impl));
}

void
ExecSystem::_CacheValues(
    const VdfSchedule &schedule,
    const VdfRequest &computeRequest)
{
    TRACE_FUNCTION();

    VdfExecutorInterface *const executor = _GetMainExecutor();

    // TODO: Change time here, if we decide to make time a parameter to
    // CacheValues().

    // Initialize the input nodes in the exec network.
    _program->InitializeInputNodes(executor);

    // Run the executor to compute the values.
    VdfExecutorErrorLogger errorLogger;
    executor->Run(schedule, computeRequest, &errorLogger);

    // Increment the executor's invalidation timestamp after each run. All
    // executor invalidation after this call will pick up the new timestamp,
    // ensuring that mung-buffer locking will take hold at invalidation edges.
    // 
    // Note, that all sub-executors must inherit the invalidation timestamp
    // (c.f., VdfExecutorInterface::InheritInvalidationTimestamp()) from their
    // parent executor for mung-buffer locking to function on sub-executors.
    executor->IncrementExecutorInvalidationTimestamp();

    // Report any errors or warnings surfaced during this executor run.
    _ReportExecutorErrors(errorLogger);
}

std::vector<VdfMaskedOutput>
ExecSystem::_Compile(TfSpan<const ExecValueKey> valueKeys)
{
    Exec_Compiler compiler(_stage, _program.get());
    return compiler.Compile(valueKeys);
}

VdfExecutorInterface *
ExecSystem::_GetMainExecutor()
{
    return _executorState->GetExecutor(_program->GetNetworkVersion());
}

void
ExecSystem::_InvalidateAll()
{
    TRACE_FUNCTION();

    _requests.clear();
    _executorState.reset();

    _program = std::make_unique<Exec_Program>();
    _executorState = std::make_unique<ExecSystem::_ExecutorState>();

    _ChangeTime(EfTime());
}

void
ExecSystem::_InvalidateAuthoredValues(
    TfSpan<ExecInvalidAuthoredValue> invalidProperties)
{
    TRACE_FUNCTION();

    const Exec_AuthoredValueInvalidationResult invalidationResult =
        _program->InvalidateAuthoredValues(invalidProperties);

    // If any of the inputs to exec changed to be time dependent when previously
    // they were not (or vice versa), we need to invalidate the main executor's
    // topological state, such that invalidation traversals pick up the new
    // time dependency.
    if (invalidationResult.isTimeDependencyChange) {
        _GetMainExecutor()->InvalidateTopologicalState();
    }

    // Notify all the requests of computed value invalidation. Not all the
    // requests will contain all the invalid leaf nodes or invalid properties,
    // and the request impls are responsible for filtering the provided
    // information.
    // 
    // TODO: Once we expect the system to contain more than a handful of
    // requests, we should do this in parallel. We might still want to invoke
    // the invalidation callbacks serially, though.
    for (const auto &requestImpl : _requests) {
        requestImpl->DidInvalidateComputedValues(invalidationResult);
    } 
}

void
ExecSystem::_ChangeTime(const EfTime &time)
{
    // Greedily initialize the time on the executor.
    const Exec_TimeChangeInvalidationResult invalidationResult =
        _program->InitializeTime(time, _GetMainExecutor());

    // Bail out if the time change did not result in invalidation, for example
    // when changing to a time that was already set, or when initializing time
    // for the first time.
    if (invalidationResult.invalidLeafNodes.empty()) {
        return;
    }

    // Notify all the requests of the time change. Not all the requests will
    // contain all the leaf nodes affected by the time change, and the request
    // impls are responsible for filtering the provided information.
    // 
    // TODO: Once we expect the system to contain more than a handful of
    // requests, we should do this in parallel. We might still want to invoke
    // the invalidation callbacks serially, though.
    for (const auto &requestImpl : _requests) {
        requestImpl->DidChangeTime(invalidationResult);
    }
}

void
ExecSystem::_ReportExecutorErrors(
    const VdfExecutorErrorLogger &errorLogger) const
{
    const VdfExecutorErrorLogger::NodeToStringMap &warnings =
        errorLogger.GetWarnings();
    if (warnings.empty()) {
        return;
    }

    TRACE_FUNCTION();

    for (auto &[node, error] : warnings) {
        if (!TF_VERIFY(node)) {
            continue;
        }

        TF_WARN("Node: '%s'. Exec Warning: %s",
            node->GetDebugName().c_str(),
            error.c_str());
    }
}

PXR_NAMESPACE_CLOSE_SCOPE
