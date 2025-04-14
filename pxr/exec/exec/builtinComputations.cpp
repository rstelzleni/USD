//
// Copyright 2025 Pixar
//
// Licensed under the terms set forth in the LICENSE.txt file available at
// https://openusd.org/license.
//
#include "pxr/exec/exec/builtinComputations.h"

PXR_NAMESPACE_OPEN_SCOPE

TfStaticData<Exec_BuiltinComputations> ExecBuiltinComputations;

// A vector of all registered builtin computation tokens.
static TfStaticData<std::vector<TfToken>> _builtinComputationNames;

TfToken
Exec_BuiltinComputations::_RegisterBuiltin(
    const std::string &name)
{
    const TfToken computationNameToken(_computationNamePrefix + name);
    _builtinComputationNames->push_back(computationNameToken);
    return computationNameToken;
}

const std::vector<TfToken> &
Exec_BuiltinComputations::GetComputationTokens()
{
    return *_builtinComputationNames;
}

PXR_NAMESPACE_CLOSE_SCOPE
