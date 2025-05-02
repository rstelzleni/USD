//
// Copyright 2025 Pixar
//
// Licensed under the terms set forth in the LICENSE.txt file available at
// https://openusd.org/license.
//
#include "pxr/exec/exec/definitionRegistry.h"

#include "pxr/exec/exec/builtinAttributeComputations.h"
#include "pxr/exec/exec/builtinComputations.h"
#include "pxr/exec/exec/builtinStageComputations.h"
#include "pxr/exec/exec/typeRegistry.h"
#include "pxr/exec/exec/types.h"

#include "pxr/base/arch/hints.h"
#include "pxr/base/tf/diagnostic.h"
#include "pxr/base/tf/instantiateSingleton.h"
#include "pxr/base/tf/stringUtils.h"
#include "pxr/base/trace/trace.h"
#include "pxr/exec/esf/attribute.h"
#include "pxr/exec/esf/prim.h"

#include <mutex>
#include <utility>

PXR_NAMESPACE_OPEN_SCOPE

TF_INSTANTIATE_SINGLETON(Exec_DefinitionRegistry);

Exec_DefinitionRegistry::Exec_DefinitionRegistry()
    : _isFullyConstructed(false)
{
    // Ensure the type registry is initialized before the definition registry so
    // that computation registrations will be able to look up value types.
    ExecTypeRegistry::GetInstance();

    // Calling SetInstanceConstructed() makes it possible to call
    // TfSingleton<>::GetInstance() before this constructor has finished.
    //
    // This is neccessary because the following call to SubscribeTo() will
    // _immediately_ invoke all registry functions which will, in turn, most
    // likely call TfSingleton<>::GetInstance().
    TfSingleton<Exec_DefinitionRegistry>::SetInstanceConstructed(*this);

    // Populate the registry with builtin computation definitions.
    _RegisterBuiltinComputations();

    // Now initialize the registry.
    //
    // We use ExecDefinitionRegistryTag to identify registry functions, rather
    // than the definition registry type, so Exec_DefinitionRegistry can remain
    // private.
    TfRegistryManager::GetInstance().SubscribeTo<ExecDefinitionRegistryTag>();

    // Callers of Exec_DefinitionRegistry::GetInstance() can now safely return
    // a fully-constructed registry.
    _SetInstanceFullyConstructed();
}

// This must be defined in the cpp file, or we get undefined symbols when
// linking.
// 
Exec_DefinitionRegistry&
Exec_DefinitionRegistry::GetInstance()
{
    Exec_DefinitionRegistry &instance =
        TfSingleton<Exec_DefinitionRegistry>::GetInstance();
    
    if (ARCH_LIKELY(instance._isFullyConstructed)) {
        return instance;
    }

    instance._WaitForFullConstruction();
    return instance;
}

Exec_DefinitionRegistry&
Exec_DefinitionRegistry::_GetInstanceForRegistration()
{
    return TfSingleton<Exec_DefinitionRegistry>::GetInstance();
}

const Exec_ComputationDefinition *
Exec_DefinitionRegistry::GetComputationDefinition(
    const EsfPrimInterface &providerPrim,
    const TfToken &computationName,
    EsfJournal *const journal) const
{
    TRACE_FUNCTION();

    const bool hasBuiltinPrefix =
        TfStringStartsWith(
            computationName.GetString(),
            Exec_BuiltinComputations::builtinComputationNamePrefix);

    // If the provider is the stage, we only support builtin computations.
    if (providerPrim.IsPseudoRoot()) {
        if (!hasBuiltinPrefix) {
            return nullptr;
        }

        const auto builtinIt =
            _builtinStageComputationDefinitions.find(computationName);
        if (builtinIt != _builtinStageComputationDefinitions.end()) {
            return builtinIt->second.get();
        }

        return nullptr;
    }

    if (hasBuiltinPrefix) {
        // Look for a prim builtin computation.
        const auto builtinIt =
            _builtinPrimComputationDefinitions.find(computationName);
        if (builtinIt != _builtinPrimComputationDefinitions.end()) {
            return builtinIt->second.get();
        }

        return nullptr;
    }

    // Otherwise, look for a plugin computation.

    const TfType schemaType = providerPrim.GetType(journal);
    if (schemaType.IsUnknown()) {
        TF_CODING_ERROR(
            "Unknown schema type when looking up definition for computation "
            "'%s'", computationName.GetText());
        return nullptr;
    }

    // Iterate over all ancestor types of the provider's schema type, from
    // derived to base, starting with the schema type itself. Look for a
    // matching plugin prim computation for the derived-most schema type that
    // defines it, or null, if no matching computation can be found.
    //
    // TODO: Repeatedly traversing the schema type hierarchy like this is
    // wasteful and we plan to cache results appropriately. But we still need to
    // add support for applied schemas, and we will also need to add the ability
    // to compose computation definitions, so for now we are leaving this
    // inefficiency in place until more of that functionality lands. The current
    // thinking is that exec will cache composed prim definitions that will be
    // keyed off of a tuple of typed and applied schemas and will cache the
    // resulting set of composed computation definitions. That will enable this
    // code to construct a key and do a single lookup into the cache, rather
    // than searching to find the computation definition.

    TfType foundType;
    std::vector<TfType> schemaAncestorTypes;
    schemaType.GetAllAncestorTypes(&schemaAncestorTypes);

    for (const TfType type : schemaAncestorTypes) {
        if (const auto pluginIt = _pluginPrimComputationDefinitions.find(
                {type, computationName});
            pluginIt != _pluginPrimComputationDefinitions.end()) {
            return &pluginIt->second;
        }
    }

    return nullptr;
}

const Exec_ComputationDefinition *
Exec_DefinitionRegistry::GetComputationDefinition(
    const EsfAttributeInterface &providerAttribute,
    const TfToken &computationName,
    EsfJournal *const journal) const
{
    // First look for a matching builtin computation.
    const auto builtinIt =
        _builtinAttributeComputationDefinitions.find(computationName);
    if (builtinIt != _builtinAttributeComputationDefinitions.end()) {
        return builtinIt->second.get();
    }

    // TODO: Look up plugin attribute computations.
    const EsfPrim owningPrim = providerAttribute.GetPrim(journal);
    const TfType primSchemaType = owningPrim->GetType(journal);
    (void)owningPrim;
    (void)primSchemaType;
    return nullptr;
}

void
Exec_DefinitionRegistry::_RegisterPrimComputation(
    TfType schemaType,
    const TfToken &computationName,
    TfType resultType,
    ExecCallbackFn &&callback,
    Exec_InputKeyVector &&inputKeys)
{
    if (schemaType.IsUnknown()) {
        TF_CODING_ERROR(
            "Attempt to register computation '%s' using an unknown type.",
            computationName.GetText());
        return;
    }

    if (TfStringStartsWith(
            computationName.GetString(),
            Exec_BuiltinComputations::builtinComputationNamePrefix)) {
        TF_CODING_ERROR(
            "Attempt to register computation '%s' with a name that uses the "
            "prefix '%s', which is reserved for builtin computations.",
            computationName.GetText(),
            Exec_BuiltinComputations::builtinComputationNamePrefix);
        return;
    }

    const bool emplaced =
        _pluginPrimComputationDefinitions.emplace(
            std::piecewise_construct, 
            std::forward_as_tuple(schemaType, computationName),
            std::forward_as_tuple(
                resultType,
                computationName,
                std::move(callback),
                std::move(inputKeys))).second;

    if (!emplaced) {
        TF_CODING_ERROR(
            "Duplicate prim computation registration for computation named "
            "'%s' on schema %s",
            computationName.GetText(),
            schemaType.GetTypeName().c_str());
    }
}

void
Exec_DefinitionRegistry::_RegisterBuiltinStageComputation(
    const TfToken &computationName,
    std::unique_ptr<Exec_ComputationDefinition> &&definition)
{
    if (!TF_VERIFY(
            TfStringStartsWith(
                computationName.GetString(),
                Exec_BuiltinComputations::builtinComputationNamePrefix))) {
        return;
    }

    const bool emplaced = 
        _builtinStageComputationDefinitions.emplace(
            computationName,
            std::move(definition)).second;

    if (!emplaced) {
        TF_CODING_ERROR(
            "Duplicate builtin computation registration for stage computation "
            "named '%s'",
            computationName.GetText());
    }
}

void
Exec_DefinitionRegistry::_RegisterBuiltinPrimComputation(
    const TfToken &computationName,
    std::unique_ptr<Exec_ComputationDefinition> &&definition)
{
    if (!TF_VERIFY(
            TfStringStartsWith(
                computationName.GetString(),
                Exec_BuiltinComputations::builtinComputationNamePrefix))) {
        return;
    }

    const bool emplaced =
        _builtinPrimComputationDefinitions.emplace(
            computationName,
            std::move(definition)).second;

    if (!emplaced) {
        TF_CODING_ERROR(
            "Duplicate builtin computation registration for prim computation "
            "named '%s'",
            computationName.GetText());
    }
}

void
Exec_DefinitionRegistry::_RegisterBuiltinAttributeComputation(
    const TfToken &computationName,
    std::unique_ptr<Exec_ComputationDefinition> &&definition)
{
    if (!TF_VERIFY(
            TfStringStartsWith(
                computationName.GetString(),
                Exec_BuiltinComputations::builtinComputationNamePrefix))) {
        return;
    }

    const bool emplaced =
        _builtinAttributeComputationDefinitions.emplace(
            computationName,
            std::move(definition)).second;

    if (!emplaced) {
        TF_CODING_ERROR(
            "Duplicate builtin attribute computation registration for "
            "computation named '%s'",
            computationName.GetText());
    }
}

void
Exec_DefinitionRegistry::_RegisterBuiltinComputations()
{
    _RegisterBuiltinStageComputation(
        ExecBuiltinComputations->computeTime,
        std::make_unique<Exec_TimeComputationDefinition>());

    _RegisterBuiltinAttributeComputation(
        ExecBuiltinComputations->computeValue,
        std::make_unique<Exec_ComputeValueComputationDefinition>());

    // Make sure we registered all builtins.
    TF_VERIFY(_builtinStageComputationDefinitions.size() +
              _builtinPrimComputationDefinitions.size() +
              _builtinAttributeComputationDefinitions.size() ==
              ExecBuiltinComputations->GetComputationTokens().size());
}

void
Exec_DefinitionRegistry::_SetInstanceFullyConstructed()
{
    // Even though _isFullyConstructed is an atomic, we still need to protect
    // its update with a lock on the mutex, or else other threads might enter
    // a wait state after we've notified the condition variable.
    std::lock_guard<std::mutex> lock(_isFullyConstructedMutex);
    _isFullyConstructed.store(true);
    _isFullyConstructedConditionVariable.notify_all();
}

void
Exec_DefinitionRegistry::_WaitForFullConstruction()
{
    TRACE_FUNCTION();

    std::unique_lock<std::mutex> lock(_isFullyConstructedMutex);
    _isFullyConstructedConditionVariable.wait(lock, [this] {
        return _isFullyConstructed.load();
    });
}

PXR_NAMESPACE_CLOSE_SCOPE
