//
// Copyright 2025 Pixar
//
// Licensed under the terms set forth in the LICENSE.txt file available at
// https://openusd.org/license.
//
#include "pxr/exec/exec/builtinsStage.h"

#include "pxr/exec/exec/builtinComputations.h"
#include "pxr/exec/exec/definitionRegistry.h"

#include "pxr/exec/ef/time.h"

PXR_NAMESPACE_OPEN_SCOPE

Exec_TimeComputationDefinition::Exec_TimeComputationDefinition()
    : Exec_ComputationDefinition(
        TfType::Find<EfTime>(),
        ExecBuiltinComputations->computeTime,
        Exec_NodeKind::TimeNode)
{
}

const Exec_InputKeyVector &
Exec_TimeComputationDefinition::GetInputKeys() const
{
    // The time node requires no inputs.
    const static Exec_InputKeyVector empty;
    return empty;
}

PXR_NAMESPACE_CLOSE_SCOPE
