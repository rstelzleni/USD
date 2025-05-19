//
// Copyright 2025 Pixar
//
// Licensed under the terms set forth in the LICENSE.txt file available at
// https://openusd.org/license.
//
#include "pxr/exec/exec/system.h"

#include "pxr/exec/exec/authoredValueInvalidationResult.h"
#include "pxr/exec/exec/compiler.h"
#include "pxr/exec/exec/disconnectedInputsInvalidationResult.h"
#include "pxr/exec/exec/program.h"
#include "pxr/exec/exec/requestImpl.h"
#include "pxr/exec/exec/timeChangeInvalidationResult.h"

#include "pxr/exec/ef/executor.h"
#include "pxr/exec/ef/time.h"
#include "pxr/exec/ef/timeInputNode.h"
#include "pxr/exec/vdf/dataManagerVector.h"
#include "pxr/exec/vdf/executorErrorLogger.h"
#include "pxr/exec/vdf/parallelDataManagerVector.h"
#include "pxr/exec/vdf/parallelExecutorEngine.h"
#include "pxr/exec/vdf/parallelSpeculationExecutorEngine.h"
#include "pxr/exec/vdf/pullBasedExecutorEngine.h"

#include "pxr/base/tf/span.h"
#include "pxr/base/trace/trace.h"
#include "pxr/base/work/loops.h"
#include "pxr/base/work/withScopedParallelism.h"

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

    // TODO: Change time here, if we decide to make time a parameter to
    // CacheValues().

    // Reset the accumulated uninitialized input nodes on the program, and
    // retain the invalidation request for executor invalidation below.
    VdfMaskedOutputVector invalidationRequest =
        _program->ResetUninitializedInputNodes();

    // Make sure that the executor data manager is properly invalidated for any
    // input nodes that were just initialized.
    VdfExecutorInterface *const executor = _GetMainExecutor();
    if (!invalidationRequest.empty()) {
        executor->InvalidateValues(invalidationRequest);
    }

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
ExecSystem::_InvalidateDisconnectedInputs()
{
    TRACE_FUNCTION();

    Exec_DisconnectedInputsInvalidationResult invalidationResult =
        _program->InvalidateDisconnectedInputs();

    // Invalidate the executor and send request invalidation.
    VdfExecutorInterface *const executor = _GetMainExecutor();
    WorkWithScopedDispatcher(
        [&executor, &invalidationResult, &requests = _requests]
        (WorkDispatcher &dispatcher){
        // Invalidate the executor data manager.
        dispatcher.Run([&](){
            if (!invalidationResult.invalidationRequest.empty()) {
                executor->InvalidateValues(
                    invalidationResult.invalidationRequest);
            }
        });

        // Notify all the requests of computed value invalidation. Not all the
        // requests will contain all the invalid leaf nodes, and the request
        // impls are responsible for filtering the provided information.
        // 
        // TODO: Once we expect the system to contain more than a handful of
        // requests, we should do this in parallel. We might still want to
        // invoke the invalidation callbacks serially, though.
        dispatcher.Run([&](){
            for (const auto &requestImpl : requests) {
                requestImpl->DidInvalidateComputedValues(invalidationResult);
            }
        });

    });
}

void
ExecSystem::_InvalidateAuthoredValues(TfSpan<const SdfPath> invalidProperties)
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
ExecSystem::_ChangeTime(const EfTime &newTime)
{
    // Retrieve the old time from the executor data manager.
    const VdfMaskedOutput timeOutput = VdfMaskedOutput(
        _program->GetTimeInputNode()->GetOutput(),
        VdfMask::AllOnes(1));
    VdfExecutorInterface *const executor = _GetMainExecutor();
    const VdfVector *const oldTimeVector = executor->GetOutputValue(
        *timeOutput.GetOutput(), timeOutput.GetMask());

    // If there isn't already a time value stored in the executor data manager,
    // perform first time initialization and return.
    if (!oldTimeVector) {
        executor->SetOutputValue(
            *timeOutput.GetOutput(),
            VdfTypedVector<EfTime>(newTime),
            timeOutput.GetMask());
        return;
    }

    // Get the old time value from the vector. If there is no change in time,
    // we can return without performing invalidation.
    const EfTime oldTime = oldTimeVector->GetReadAccessor<EfTime>()[0];
    if (oldTime == newTime) {
        return;
    }

    TRACE_FUNCTION();

    // Invalidate time on the program.
    const Exec_TimeChangeInvalidationResult invalidationResult =
        _program->InvalidateTime(oldTime, newTime);

    // Invalidate the executor and send request invalidation notification.
    WorkWithScopedDispatcher(
        [&executor, &invalidationResult, &timeOutput, &newTime,
             &requests = _requests]
        (WorkDispatcher &dispatcher){
        // Invalidate values on the executor and set the new time.
        if (!invalidationResult.invalidationRequest.empty()) {
            dispatcher.Run([&](){     
                executor->InvalidateValues(
                    invalidationResult.invalidationRequest);
                executor->SetOutputValue(
                    *timeOutput.GetOutput(),
                    VdfTypedVector<EfTime>(newTime),
                    timeOutput.GetMask());
            });
        }

        // Notify all the requests of the time change. Not all the requests will
        // contain all the leaf nodes affected by the time change, and the
        // request impls are responsible for filtering the provided information.
        // 
        // TODO: Once we expect the system to contain more than a handful of
        // requests, we should do this in parallel. We might still want to
        // invoke the invalidation callbacks serially, though.
        if (!invalidationResult.invalidLeafNodes.empty()) {
            dispatcher.Run([&](){
                for (const auto &requestImpl : requests) {
                    requestImpl->DidChangeTime(invalidationResult);
                }
            });
        }
    });
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
