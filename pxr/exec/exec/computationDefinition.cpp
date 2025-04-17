//
// Copyright 2025 Pixar
//
// Licensed under the terms set forth in the LICENSE.txt file available at
// https://openusd.org/license.
//
#include "pxr/exec/exec/computationDefinition.h"

#include <utility>

PXR_NAMESPACE_OPEN_SCOPE

//
// Exec_ComputationDefinition
//

Exec_ComputationDefinition::Exec_ComputationDefinition(
    TfType resultType,
    const TfToken &computationName,
    Exec_NodeKind category)
    : _resultType(resultType)
    , _computationName(computationName)
    , _nodeKind(category)
{        
}

const ExecCallbackFn &
Exec_ComputationDefinition::GetCallback() const {
    static const ExecCallbackFn emptyCallback;
    return emptyCallback;
}

//
// Exec_PluginComputationDefinition
//

Exec_PluginComputationDefinition::Exec_PluginComputationDefinition(
    TfType resultType,
    const TfToken &computationName,
    ExecCallbackFn &&callback,
    Exec_InputKeyVector &&inputKeys)
    : Exec_ComputationDefinition(
        resultType,
        computationName,
        Exec_NodeKind::Callback)
    , _callback(std::move(callback))
    , _inputKeys(std::move(inputKeys))
{
}

const ExecCallbackFn &
Exec_PluginComputationDefinition::GetCallback() const {
    return _callback;
}

const Exec_InputKeyVector &
Exec_PluginComputationDefinition::GetInputKeys() const {
    return _inputKeys;
}

PXR_NAMESPACE_CLOSE_SCOPE
