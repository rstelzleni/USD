//
// Copyright 2025 Pixar
//
// Licensed under the terms set forth in the LICENSE.txt file available at
// https://openusd.org/license.
//
#include "pxr/exec/exec/outputProvidingCompilationTask.h"

#include "pxr/exec/exec/compilationState.h"
#include "pxr/exec/exec/computationDefinition.h"
#include "pxr/exec/exec/inputKey.h"
#include "pxr/exec/exec/inputResolvingCompilationTask.h"
#include "pxr/exec/exec/program.h"

#include "pxr/base/tf/token.h"
#include "pxr/base/trace/trace.h"
#include "pxr/exec/esf/journal.h"

PXR_NAMESPACE_OPEN_SCOPE

void
Exec_OutputProvidingCompilationTask::_Compile(
    Exec_CompilationState &compilationState,
    TaskStages &taskStages)
{
    TRACE_FUNCTION();

    const Exec_ComputationDefinition *const computationDefinition =
        _outputKey.GetComputationDefinition();
    const Exec_InputKeyVector &inputKeys =
        computationDefinition->GetInputKeys();

    taskStages.Invoke(
    // Make sure input dependencies are fulfilled
    [this, &compilationState, &inputKeys](TaskDependencies &deps) {
        TRACE_FUNCTION_SCOPE("input tasks");

        _inputSources.resize(inputKeys.size());
        _inputJournals.resize(inputKeys.size());
        const size_t numInputKeys = inputKeys.size();
        for (size_t i = 0; i < numInputKeys; ++i) {
            deps.NewSubtask<Exec_InputResolvingCompilationTask>(
                compilationState,
                inputKeys[i],
                _outputKey.GetProviderObject(),
                &_inputSources[i],
                &_inputJournals[i]);
        }
    },

    // Compile and connect the callback node
    [this, &compilationState, computationDefinition, &inputKeys](
        TaskDependencies &deps) {
        TRACE_FUNCTION_SCOPE("node creation");

        // TODO: Journaling
        EsfJournal nodeJournal;

        VdfNode *const node = computationDefinition->CompileNode(
            nodeJournal, compilationState.GetProgram());

        if (!TF_VERIFY(node)) {
            return;
        }

        const Exec_OutputKey::Identity keyIdentity = _outputKey.MakeIdentity();
        node->SetDebugNameCallback([keyIdentity]{
            return keyIdentity.GetDebugName();
        });

        for (size_t i = 0; i < _inputSources.size(); ++i) {
            compilationState.GetProgram()->Connect(
                _inputJournals[i],
                _inputSources[i],
                node,
                inputKeys[i].inputName);
        }

        // Return the compiled output to the calling task.
        VdfMaskedOutput compiledOutput(node->GetOutput(), VdfMask::AllOnes(1));
        *_resultOutput = compiledOutput;

        // Then publish it to the compiled outputs cache.
        TF_VERIFY(compilationState.GetProgram()->SetCompiledOutput(
            _outputKey.MakeIdentity(), compiledOutput));

        // Then indicate that the task identified by _outputKey is done. This
        // notifies all other tasks with a dependency on this _outputKey.
        _MarkDone(_outputKey.MakeIdentity());
    }
    );
}

PXR_NAMESPACE_CLOSE_SCOPE
