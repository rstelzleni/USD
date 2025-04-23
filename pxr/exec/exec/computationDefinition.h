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

class EsfJournal;
class Exec_Program;
class VdfNode;

/// A base class that defines an exec computation.
///
/// A computation definition includes a VdfNode kind and an optional callback
/// that implements the evaluation logic, if the kind is Callback.
///
/// A computation definition can also produce input keys that describe how to
/// source the input values the computation requires.
///
class Exec_ComputationDefinition
{
public:
    Exec_ComputationDefinition(
        const Exec_ComputationDefinition &) = delete;
    Exec_ComputationDefinition &operator=(
        const Exec_ComputationDefinition &) = delete;

    virtual ~Exec_ComputationDefinition();

    /// Returns the value type that is produced by this computation.
    ///
    TfType GetResultType() const {
        return _resultType;
    }

    /// Returns the name of the computation.
    ///
    const TfToken &GetComputationName() const {
        return _computationName;
    }

    /// Returns the keys that indicate how to source the input values required
    /// to evaluate the computation defined by this definition.
    ///
    virtual const Exec_InputKeyVector &GetInputKeys() const = 0;

    /// Compiles the node that implements the computation, adding it to the
    /// network owned by \program.
    ///
    /// The information in \p nodeJournal will be used to determine when the
    /// node must be uncompiled.
    ///
    virtual VdfNode *CompileNode(
        const EsfJournal &nodeJournal,
        Exec_Program *program) const = 0;

protected:
    /// Creates a definition for a computation.
    ///
    Exec_ComputationDefinition(
        TfType resultType,
        const TfToken &computationName);

private:
    const TfType _resultType;
    const TfToken _computationName;
};

/// A class that defines a plugin computation.
///
/// A plugin computation definition includes the callback that implements the
/// evaluation logic.
///
class Exec_PluginComputationDefinition final
    : public Exec_ComputationDefinition
{
public:
    /// Creates a definition for a computation.
    ///
    /// The computation's evaluation-time behavior is implemented in \p
    /// callback. The \p inputKeys indicate how to source the input values that
    /// are required at evaluation time.
    ///
    Exec_PluginComputationDefinition(
        TfType resultType,
        const TfToken &computationName,
        ExecCallbackFn &&callback,
        Exec_InputKeyVector &&inputKeys);

    ~Exec_PluginComputationDefinition() override;

    const Exec_InputKeyVector &GetInputKeys() const override;

    VdfNode *CompileNode(
        const EsfJournal &nodeJournal,
        Exec_Program *program) const override;

private:
    const ExecCallbackFn _callback;
    const Exec_InputKeyVector _inputKeys;
};

PXR_NAMESPACE_CLOSE_SCOPE

#endif
