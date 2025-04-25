//
// Copyright 2025 Pixar
//
// Licensed under the terms set forth in the LICENSE.txt file available at
// https://openusd.org/license.
//
#include "pxr/exec/exec/system.h"

#include "pxr/exec/exec/compiler.h"
#include "pxr/exec/exec/program.h"
#include "pxr/exec/exec/requestImpl.h"

#include "pxr/exec/ef/executor.h"
#include "pxr/exec/vdf/executorErrorLogger.h"
#include "pxr/exec/vdf/parallelDataManagerVector.h"
#include "pxr/exec/vdf/parallelExecutorEngine.h"
#include "pxr/exec/vdf/parallelSpeculationExecutorEngine.h"

#include "pxr/base/tf/span.h"
#include "pxr/base/trace/trace.h"

#include <utility>

PXR_NAMESPACE_OPEN_SCOPE

// Encapsulates the state of the main executor and performs topological state
// invalidation before handing out access to the executor.
// 
class ExecSystem::_ExecutorState 
{
public:
    template <class T, class... Args>
    void CreateExecutor(Args... args) {
        _executor = std::make_unique<T>(std::forward<Args>(args)...);
    }

    VdfExecutorInterface *GetExecutor(size_t networkVersion);

private:
    std::unique_ptr<VdfExecutorInterface> _executor = nullptr;
    size_t _lastNetworkVersion = 0;
};

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
{
    _CreateExecutorState();
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

    // TODO: Initialize program

    VdfExecutorErrorLogger errorLogger;
    VdfExecutorInterface *const executor = _GetMainExecutor();
    executor->Run(schedule, computeRequest, &errorLogger);

    // Increment the executor's invalidation timestamp after each run. All
    // executor invalidation after this call will pick up the new timestamp,
    // ensuring that mung-buffer locking will take hold at invalidation edges.
    // 
    // Note, that all sub-executors must inherit the invalidation timestamp
    // (c.f., VdfExecutorInterface::InheritInvalidationTimestamp()) from their
    // parent executor for mung-buffer locking to function on sub-executors.
    executor->IncrementExecutorInvalidationTimestamp();

    _ReportExecutorErrors(errorLogger);
}

std::vector<VdfMaskedOutput>
ExecSystem::_Compile(TfSpan<const ExecValueKey> valueKeys)
{
    Exec_Compiler compiler(_stage, _program.get());
    return compiler.Compile(valueKeys);
}

void
ExecSystem::_CreateExecutorState()
{
    _executorState = std::make_unique<ExecSystem::_ExecutorState>();
    _executorState->CreateExecutor<
        EfExecutor<VdfParallelExecutorEngine, VdfParallelDataManagerVector>>();
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
    _CreateExecutorState();
}

void
ExecSystem::_InvalidateAuthoredValues(
    TfSpan<ExecInvalidAuthoredValue> invalidProperties)
{
    TRACE_FUNCTION();

    _program->InvalidateAuthoredValues(invalidProperties);

    //TODO: Send request invalidation
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
