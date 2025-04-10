//
// Copyright 2025 Pixar
//
// Licensed under the terms set forth in the LICENSE.txt file available at
// https://openusd.org/license.
//
#include "pxr/exec/exec/leafCompilationTask.h"

#include "pxr/exec/exec/compilationState.h"
#include "pxr/exec/exec/inputKey.h"
#include "pxr/exec/exec/inputResolvingCompilationTask.h"
#include "pxr/exec/exec/program.h"

#include "pxr/base/trace/trace.h"
#include "pxr/exec/ef/leafNode.h"
#include "pxr/exec/esf/journal.h"
#include "pxr/exec/esf/object.h"
#include "pxr/exec/esf/stage.h"
#include "pxr/exec/vdf/maskedOutput.h"

PXR_NAMESPACE_OPEN_SCOPE

static Exec_InputKey _MakeInputKey(const ExecValueKey &valueKey);

void
Exec_LeafCompilationTask::_Compile(
    Exec_CompilationState &compilationState,
    TaskStages &taskStages)
{
    TRACE_FUNCTION();

    taskStages.Invoke(
    // Turn the value key into an input key and create an input resolving
    // subtask to compile the source output to later connect to the leaf node.
    [this, &compilationState]
    (TaskDependencies &deps) {
        TRACE_FUNCTION_SCOPE("input compilation");

        // TODO: Journaling
        EsfJournal inputJournal;

        // Get the provider object from the value key
        const EsfStage &stage = compilationState.GetStage();
        const EsfObject providerObject =
            stage->GetObjectAtPath(_valueKey.GetProviderPath(), &inputJournal);

        // Make an input key from the value key
        const Exec_InputKey inputKey = _MakeInputKey(_valueKey);

        // Run a new subtask to compile the input
        deps.NewSubtask<Exec_InputResolvingCompilationTask>(
            compilationState,
            inputKey,
            providerObject,
            &_resultOutputs);
    },

    // Compile and connect the leaf node.
    [this, &compilationState]
    (TaskDependencies &deps) {
        TRACE_FUNCTION_SCOPE("leaf node creation");

        if (!TF_VERIFY(_resultOutputs.size() == 1)) {
            return;
        }

        const VdfMaskedOutput &sourceOutput = _resultOutputs.front();
        if (!TF_VERIFY(sourceOutput)) {
            return;
        }

        // Return the compiled source output as the requested leaf output
        *_leafOutput = sourceOutput;

        // TODO: Journaling
        EsfJournal nodeJournal;
        EsfJournal inputJournal;

        EfLeafNode *const leafNode =
            compilationState.GetProgram()->CreateNode<EfLeafNode>(
                nodeJournal,
                sourceOutput.GetOutput()->GetSpec().GetType());

        leafNode->SetDebugNameCallback([valueKey = _valueKey]{
            return valueKey.GetDebugName();
        });

        compilationState.GetProgram()->Connect(
            inputJournal,
            TfSpan<const VdfMaskedOutput>(&sourceOutput, 1),
            leafNode,
            EfLeafTokens->in);
    }
    );
}

Exec_InputKey _MakeInputKey(const ExecValueKey &valueKey)
{
    return {
        EfLeafTokens->in,
        valueKey.GetComputationToken(),
        TfType(),
        {SdfPath::ReflexiveRelativePath(),
            ExecProviderResolution::DynamicTraversal::Local},
        false /* optional */
    };
}

PXR_NAMESPACE_CLOSE_SCOPE
