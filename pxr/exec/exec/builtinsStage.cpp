//
// Copyright 2025 Pixar
//
// Licensed under the terms set forth in the LICENSE.txt file available at
// https://openusd.org/license.
//
#include "pxr/exec/exec/builtinsStage.h"

#include "pxr/exec/exec/builtinComputations.h"
#include "pxr/exec/exec/definitionRegistry.h"
#include "pxr/exec/exec/program.h"

#include "pxr/base/tf/diagnosticLite.h"
#include "pxr/exec/ef/time.h"
#include "pxr/exec/ef/timeInputNode.h"

PXR_NAMESPACE_OPEN_SCOPE

Exec_TimeComputationDefinition::Exec_TimeComputationDefinition()
    : Exec_ComputationDefinition(
        TfType::Find<EfTime>(),
        ExecBuiltinComputations->computeTime)
{
}

Exec_TimeComputationDefinition::~Exec_TimeComputationDefinition() = default;

const Exec_InputKeyVector &
Exec_TimeComputationDefinition::GetInputKeys() const
{
    // The time node requires no inputs.
    const static Exec_InputKeyVector empty;
    return empty;
}

VdfNode *
Exec_TimeComputationDefinition::CompileNode(
    const EsfJournal &nodeJournal,
    Exec_Program *const program) const
{
    return program->CreateNode<EfTimeInputNode>(nodeJournal);
}

PXR_NAMESPACE_CLOSE_SCOPE
