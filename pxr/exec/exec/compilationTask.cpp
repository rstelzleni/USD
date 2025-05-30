//
// Copyright 2025 Pixar
//
// Licensed under the terms set forth in the LICENSE.txt file available at
// https://openusd.org/license.
//
#include "pxr/exec/exec/compilationTask.h"

#include "pxr/exec/exec/compilationState.h"

PXR_NAMESPACE_OPEN_SCOPE

Exec_CompilationTask::~Exec_CompilationTask() = default;

Exec_CompilerTaskSync::ClaimResult
Exec_CompilationTask::TaskDependencies::ClaimSubtask(
    const Exec_OutputKey::Identity &key)
{
    const Exec_CompilerTaskSync::ClaimResult result =
        Exec_CompilationState::OutputTasksAccess::_Get(&_compilationState)
            .Claim(key, _task);
    if (result == Exec_CompilerTaskSync::ClaimResult::Wait) {
        _hasDependencies = true;
    }
    return result;
}

void
Exec_CompilationTask::operator()() const
{
    // WorkDispatcher semantics require call operators to be const, but we need
    // to mutate our internal task state.
    Exec_CompilationTask *thisTask = const_cast<Exec_CompilationTask*>(this);

    // Register an additional dependency while this task is running.
    // 
    // This ensures that if sub-tasks complete while this task is still running,
    // the last completed sub-task will not re-run this task and cause it to be
    // re-entrant before we get to the end of this method. We undo this below by
    // calling RemoveDependency().
    thisTask->AddDependency();

    // Call the _Compile() method, which is the main entry point into
    // compilation tasks.
    TaskPhases taskPhases(
        thisTask, thisTask->_compilationState, thisTask->_taskPhase);
    thisTask->_Compile(_compilationState, taskPhases);

    // If the task *did not* complete, there are additional phases to run, and
    // one or more sub-tasks constituting unfulfilled dependencies haven't run
    // to completion.
    if (!taskPhases._IsComplete()) {
        // Let's remove the dependency we added above to prevent re-entry.
        // 
        // After this line, the last completed dependency will immediately re-
        // run this task - so we *must* return right after. However, if we
        // happen to remove the last remaining dependency here, we are on the
        // hook to re-run this task.
        if (thisTask->RemoveDependency() == 0) {
            Exec_CompilationState::OutputTasksAccess::_Get(&_compilationState)
                .Run(thisTask);
        }
        return;
    }

    // If the task *did* complete, and it is a sub-task, we need to remove one
    // dependency from the parent task.
    if (Exec_CompilationTask *const parent = thisTask->_parent) {
        // If we remove the last unfulfilled dependency from the parent task,
        // the parent is ready to re-run. We're responsible for making that
        // happen here.
        if (parent->RemoveDependency() == 0) {
            Exec_CompilationState::OutputTasksAccess::_Get(&_compilationState)
                .Run(parent);
        }
    }

    // The task just completed, and tasks manage their own lifetime: We must
    // delete it now.
    delete thisTask;
}

void
Exec_CompilationTask::_MarkDone(const Exec_OutputKey::Identity &key)
{
    Exec_CompilationState::OutputTasksAccess::_Get(&_compilationState)
        .MarkDone(key);
}

PXR_NAMESPACE_CLOSE_SCOPE
