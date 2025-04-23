//
// Copyright 2025 Pixar
//
// Licensed under the terms set forth in the LICENSE.txt file available at
// https://openusd.org/license.
//
#ifndef PXR_EXEC_EXEC_BUILTIN_STAGE_COMPUTATIONS_H
#define PXR_EXEC_EXEC_BUILTIN_STAGE_COMPUTATIONS_H

#include "pxr/exec/exec/computationDefinition.h"
#include "pxr/exec/exec/inputKey.h"

PXR_NAMESPACE_OPEN_SCOPE

//
// This file defines builtin computations that are provided by the stage, i.e.,
// by the pseudo-root prim.
//

/// A computation that yields the current evaluation time.
class Exec_TimeComputationDefinition final
    : public Exec_ComputationDefinition
{
public:
    Exec_TimeComputationDefinition();
    
    ~Exec_TimeComputationDefinition() override;

    const Exec_InputKeyVector &GetInputKeys() const override;

    VdfNode *CompileNode(
        const EsfJournal &nodeJournal,
        Exec_Program *program) const override;
};

PXR_NAMESPACE_CLOSE_SCOPE

#endif
