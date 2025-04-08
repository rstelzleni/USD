//
// Copyright 2025 Pixar
//
// Licensed under the terms set forth in the LICENSE.txt file available at
// https://openusd.org/license.
//
#ifndef PXR_EXEC_EXEC_COMPUTATION_DEFINITION_H
#define PXR_EXEC_EXEC_COMPUTATION_DEFINITION_H

#include "pxr/pxr.h"

#include "pxr/exec/exec/inputKey.h"
#include "pxr/exec/exec/types.h"

#include "pxr/base/tf/type.h"
#include "pxr/base/tf/token.h"

PXR_NAMESPACE_OPEN_SCOPE

class VdfContext;

/// A class that defines an exec computation.
///
/// A computation definition includes the callback that implements the
/// evaluation logic and descriptions of how to source the input values the
/// computation requires.
///
class Exec_ComputationDefinition
{
public:

    /// Creates a definition for a computation.
    ///
    /// The computation's evaluation-time behavior is implemented in \p
    /// callback. The \p inputKeys indicate how to source the input values that
    /// are required at evaluation time.
    Exec_ComputationDefinition(
        TfType resultType,
        const TfToken &computationName,
        ExecCallbackFn &&callback,
        Exec_InputKeyVector &&inputKeys);

    Exec_ComputationDefinition(
        const Exec_ComputationDefinition &) = delete;
    Exec_ComputationDefinition &operator=(
        const Exec_ComputationDefinition &) = delete;

    /// Returns the result type of this computation.
    TfType GetResultType() const {
        return _resultType;
    }

    /// Returns the token name of the computation.
    const TfToken &GetComputationName() const {
        return _computationName;
    }

    /// Returns the callback function that implements the logic for the
    /// computation defined by this definition.
    const ExecCallbackFn &GetCallback() const {
        return _callback;
    }

    /// Returns the keys that indicate how to source the input values required
    /// to evaluate the computation defined by this definition.
    const Exec_InputKeyVector &GetInputKeys() const {
        return _inputKeys;
    }

private:
    
    const TfType _resultType;
    const TfToken _computationName;
    const ExecCallbackFn _callback;
    const Exec_InputKeyVector _inputKeys;
};

PXR_NAMESPACE_CLOSE_SCOPE

#endif
