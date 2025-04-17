//
// Copyright 2025 Pixar
//
// Licensed under the terms set forth in the LICENSE.txt file available at
// https://openusd.org/license.
//
#include "pxr/exec/exec/builtinComputations.h"

#include "pxr/exec/exec/definitionRegistry.h"

#include "pxr/exec/ef/time.h"

PXR_NAMESPACE_OPEN_SCOPE

TfStaticData<Exec_BuiltinComputations> ExecBuiltinComputations;

// A vector of all registered builtin computation tokens.
static TfStaticData<std::vector<TfToken>> _builtinComputationNames;

static TfToken
_RegisterBuiltin(const std::string &name)
{
    static constexpr char _computationNamePrefix[] = "__";

    const TfToken computationNameToken(_computationNamePrefix + name);
    _builtinComputationNames->push_back(computationNameToken);
    return computationNameToken;
}

Exec_BuiltinComputations::Exec_BuiltinComputations()
    : computeTime(_RegisterBuiltin("computeTime"))
    , computeValue(_RegisterBuiltin("computeValue"))
{
}

const std::vector<TfToken> &
Exec_BuiltinComputations::GetComputationTokens()
{
    return *_builtinComputationNames;
}


namespace {

// A computation that provides the current evaluation time.
struct Exec_TimeComputationDefinition final : public Exec_ComputationDefinition
{
    Exec_TimeComputationDefinition()
        : Exec_ComputationDefinition(
            TfType::Find<EfTime>(),
            ExecBuiltinComputations->computeTime,
            Exec_NodeKind::TimeNode)
    {
    }

    // The time node requires no inputs.
    const Exec_InputKeyVector &GetInputKeys() const override {
        const static Exec_InputKeyVector empty;
        return empty;
    }
};

} // anonymous namespace


void
Exec_BuiltinComputations::_PopulateBuiltinComputations()
{
    Exec_DefinitionRegistry::RegisterBuiltinComputationAccess::_Register(
        ExecBuiltinComputations->computeTime,
        new Exec_TimeComputationDefinition());

    // TODO: Register computeValue
}

PXR_NAMESPACE_CLOSE_SCOPE
