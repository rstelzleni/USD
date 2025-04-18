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

        // Get the provider object from the value key
        _originObject = _valueKey.GetProvider();

        // Make an input key from the value key
        const Exec_InputKey inputKey = _MakeInputKey(_valueKey);

        // Run a new subtask to compile the input
        deps.NewSubtask<Exec_InputResolvingCompilationTask>(
            compilationState,
            inputKey,
            *_originObject,
            &_resultOutputs,
            &_journal);
    },

    // Compile and connect the leaf node.
    [this, &compilationState]
    (TaskDependencies &deps) {
        TRACE_FUNCTION_SCOPE("leaf node creation");

        if (!TF_VERIFY(_resultOutputs.size() == 1,
                "Expected exactly one output for value key '%s'; got '%zu'",
                _valueKey.GetDebugName().c_str(),
                _resultOutputs.size())) {
            return;
        }

        const VdfMaskedOutput &sourceOutput = _resultOutputs.front();
        if (!TF_VERIFY(sourceOutput)) {
            return;
        }

        // Return the compiled source output as the requested leaf output
        *_leafOutput = sourceOutput;

        // TODO: Determine what journals need to be used to create leaf nodes.
        const EsfJournal nodeJournal;

        EfLeafNode *const leafNode =
            compilationState.GetProgram()->CreateNode<EfLeafNode>(
                nodeJournal,
                sourceOutput.GetOutput()->GetSpec().GetType());

        // Value keys are not durable across scene changes so their debug name
        // must be collected eagerly.
        leafNode->SetDebugName(_valueKey.GetDebugName());

        compilationState.GetProgram()->Connect(
            _journal,
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
        valueKey.GetComputationName(),
        TfType(),
        {SdfPath::ReflexiveRelativePath(),
            ExecProviderResolution::DynamicTraversal::Local},
        false /* optional */
    };
}

PXR_NAMESPACE_CLOSE_SCOPE
