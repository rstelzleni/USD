//
// Copyright 2025 Pixar
//
// Licensed under the terms set forth in the LICENSE.txt file available at
// https://openusd.org/license.
//
#include "pxr/exec/exec/compiler.h"

#include "pxr/exec/exec/compilationState.h"
#include "pxr/exec/exec/inputRecompilationTask.h"
#include "pxr/exec/exec/leafCompilationTask.h"
#include "pxr/exec/exec/program.h"
#include "pxr/exec/exec/valueKey.h"

#include "pxr/base/tf/span.h"
#include "pxr/base/trace/trace.h"
#include "pxr/base/work/loops.h"
#include "pxr/base/work/withScopedParallelism.h"
#include "pxr/exec/vdf/maskedOutput.h"

#include <unordered_set>

PXR_NAMESPACE_OPEN_SCOPE

static void
_SpawnLeafCompilationTask(
    tbb::empty_task *const rootTask,
    Exec_CompilationState &compilationState,
    const ExecValueKey &valueKey,
    VdfMaskedOutput *const leafOutput)
{
    tbb::task *requestedOutputTask =
        new (tbb::task::allocate_additional_child_of(*rootTask))
            Exec_LeafCompilationTask(compilationState, valueKey, leafOutput);
    tbb::task::spawn(*requestedOutputTask);
}

static void
_SpawnInputRecompilationTask(
    tbb::empty_task *const rootTask,
    Exec_CompilationState &compilationState,
    VdfInput *const input)
{
    tbb::task *inputRecompilationTask =
        new (tbb::task::allocate_additional_child_of(*rootTask))
            Exec_InputRecompilationTask(compilationState, input);
    tbb::task::spawn(*inputRecompilationTask);
}

Exec_Compiler::Exec_Compiler(
    const EsfStage &stage,
    Exec_Program *program)
    : _stage(stage)
    , _program(program)
    , _rootTask(nullptr)
    , _taskGroupContext(
        tbb::task_group_context::isolated,
        tbb::task_group_context::concurrent_wait |
        tbb::task_group_context::default_traits)
{
    _rootTask = new (tbb::task::allocate_root(_taskGroupContext))
        tbb::empty_task();
    _rootTask->set_ref_count(1);
}

Exec_Compiler::~Exec_Compiler()
{
    tbb::task::destroy(*_rootTask);
}

std::vector<VdfMaskedOutput>
Exec_Compiler::Compile(TfSpan<const ExecValueKey> valueKeys)
{
    TRACE_FUNCTION();

    // Note that the returned vector should always have the same size as
    // valueKeys.  Any key that failed to compile should yield a null masked
    // output at the corresponding index in the result.
    std::vector<VdfMaskedOutput> leafOutputs(valueKeys.size());

    // These VdfInputs have been disconnected by previous rounds of
    // uncompilation and need to be recompiled.
    const std::unordered_set<VdfInput *> &inputsRequiringRecompilation =
        _program->GetInputsRequiringRecompilation();

    // Compiler state shared between all compilation tasks.
    Exec_CompilationState state(_stage, _program);

    // Process requested value keys in parallel and spawn compilation tasks.
    WorkWithScopedParallelism(
        [rootTask = _rootTask, &state, valueKeys, &leafOutputs,
        &inputsRequiringRecompilation]() {

        // TODO: We currently compile duplicate leaf nodes if multiple requests
        // contain the same value key, or if the same request is compiled more
        // than once. We need a mechanism to prevent this from happening.
        WorkParallelForN(valueKeys.size(),
            [rootTask, &state, valueKeys, &leafOutputs](size_t b, size_t e) {
            for (size_t i = b; i != e; ++i) {
                _SpawnLeafCompilationTask(
                    rootTask, state, valueKeys[i], &leafOutputs[i]);
            }
        });

        WorkParallelForN(inputsRequiringRecompilation.bucket_count(),
            [rootTask, &state, &inputsRequiringRecompilation]
            (size_t b, size_t e) {
                for (size_t bucketIdx = b; bucketIdx != e; ++bucketIdx) {
                    auto bucketIter =
                        inputsRequiringRecompilation.cbegin(bucketIdx);
                    const auto bucketEnd =
                        inputsRequiringRecompilation.cend(bucketIdx);
                    for (; bucketIter != bucketEnd; ++bucketIter) {
                        _SpawnInputRecompilationTask(
                            rootTask, state, *bucketIter);
                    }
                }
        });

        {
            TRACE_FUNCTION_SCOPE("waiting for tasks");
            rootTask->wait_for_all();
        }
    });

    // All inputs requiring recompilation have been recompiled.
    _program->ClearInputsRequiringRecompilation();

    return leafOutputs;
}

PXR_NAMESPACE_CLOSE_SCOPE
