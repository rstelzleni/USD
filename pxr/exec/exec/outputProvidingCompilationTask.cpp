//
// Copyright 2025 Pixar
//
// Licensed under the terms set forth in the LICENSE.txt file available at
// https://openusd.org/license.
//
#include "pxr/exec/exec/outputProvidingCompilationTask.h"

#include "pxr/exec/exec/callbackNode.h"
#include "pxr/exec/exec/compilationState.h"
#include "pxr/exec/exec/compiledOutputCache.h"
#include "pxr/exec/exec/computationDefinition.h"
#include "pxr/exec/exec/inputKey.h"
#include "pxr/exec/exec/inputResolvingCompilationTask.h"
#include "pxr/exec/exec/program.h"

#include "pxr/base/tf/token.h"
#include "pxr/base/trace/trace.h"
#include "pxr/exec/esf/journal.h"
#include "pxr/exec/vdf/connectorSpecs.h"
#include "pxr/exec/vdf/tokens.h"

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
        _SourceOutputs *sourceOutputs = _inputSources.data();
        for (const Exec_InputKey &inputKey : inputKeys) {
            deps.NewSubtask<Exec_InputResolvingCompilationTask>(
                compilationState,
                inputKey,
                _outputKey.GetProviderObject(),
                sourceOutputs++);
        }
    },

    // Compile and connect the callback node
    [this, &compilationState, computationDefinition, &inputKeys](
        TaskDependencies &deps) {
        TRACE_FUNCTION_SCOPE("node creation");

        VdfInputSpecs inputSpecs;
        inputSpecs.Reserve(inputKeys.size());
        for (const Exec_InputKey &inputKey : inputKeys) {
            inputSpecs.ReadConnector(inputKey.resultType, inputKey.inputName);
        }

        VdfOutputSpecs outputSpecs;
        outputSpecs.Connector(
            computationDefinition->GetResultType(), VdfTokens->out);

        // TODO: Journaling
        EsfJournal nodeJournal;

        VdfNode *const callbackNode =
            compilationState.GetProgram()->CreateNode<Exec_CallbackNode>(
                nodeJournal,
                inputSpecs,
                outputSpecs,
                computationDefinition->GetCallback());

        const Exec_OutputKey::Identity keyIdentity = _outputKey.MakeIdentity();
        callbackNode->SetDebugNameCallback([keyIdentity]{
            return keyIdentity.GetDebugName();
        });

        for (size_t i = 0; i < _inputSources.size(); ++i) {
            // TODO: Journaling
            EsfJournal inputJournal;

            compilationState.GetProgram()->Connect(
                inputJournal,
                _inputSources[i],
                callbackNode,
                inputKeys[i].inputName);
        }

        // Return the compiled output to the calling task.
        VdfMaskedOutput compiledOutput(
            callbackNode->GetOutput(), VdfMask::AllOnes(1));
        *_resultOutput = compiledOutput;

        // Then publish it to the compiled outputs cache.
        Exec_CompiledOutputCache *compiledOutputs =
            compilationState.GetProgram()->GetCompiledOutputCache();
        compiledOutputs->Insert(_outputKey.MakeIdentity(), compiledOutput);

        // Then indicate that the task identified by _outputKey is done. This
        // notifies all other tasks with a dependency on this _outputKey.
        _MarkDone(_outputKey.MakeIdentity());
    }
    );
}

PXR_NAMESPACE_CLOSE_SCOPE
