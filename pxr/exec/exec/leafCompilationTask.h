//
// Copyright 2025 Pixar
//
// Licensed under the terms set forth in the LICENSE.txt file available at
// https://openusd.org/license.
//
#ifndef PXR_EXEC_EXEC_LEAF_COMPILATION_TASK_H
#define PXR_EXEC_EXEC_LEAF_COMPILATION_TASK_H

#include "pxr/pxr.h"

#include "pxr/exec/exec/api.h"

#include "pxr/exec/exec/compilationTask.h"
#include "pxr/exec/exec/valueKey.h"

#include "pxr/base/tf/smallVector.h"

PXR_NAMESPACE_OPEN_SCOPE

class Exec_CompilationState;
class VdfMaskedOutput;

/// Leaf compilation task for compiling requested outputs.
///
/// This is the main entry point into the compilation task graph for outputs
/// that have been requested via an ExecRequest and therefore need leaf nodes
/// compiled and connected to them.
class Exec_LeafCompilationTask : public Exec_CompilationTask
{
public:
    Exec_LeafCompilationTask(
        Exec_CompilationState &compilationState,
        const ExecValueKey &valueKey,
        VdfMaskedOutput *leafOutput) :
        Exec_CompilationTask(compilationState),
        _valueKey(valueKey),
        _leafOutput(leafOutput)
    {}

private:
    void _Compile(
        Exec_CompilationState &compilationState,
        TaskStages &taskStages) override;

    // The value key for the requested output.
    const ExecValueKey _valueKey;

    // The array of outputs populated by the input resolving task.
    TfSmallVector<VdfMaskedOutput, 1> _resultOutputs;

    // Pointer to the leaf output to be populated by this task.
    VdfMaskedOutput *const _leafOutput;
};

PXR_NAMESPACE_CLOSE_SCOPE

#endif