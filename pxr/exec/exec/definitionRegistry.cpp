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
#include "pxr/exec/exec/registrationBarrier.h"
#include "pxr/exec/exec/typeRegistry.h"
#include "pxr/exec/exec/types.h"

#include "pxr/base/arch/hints.h"
#include "pxr/base/tf/diagnostic.h"
#include "pxr/base/tf/instantiateSingleton.h"
#include "pxr/base/tf/stringUtils.h"
#include "pxr/base/trace/trace.h"
#include "pxr/exec/esf/attribute.h"
#include "pxr/exec/esf/prim.h"

#include <utility>

PXR_NAMESPACE_OPEN_SCOPE

TF_INSTANTIATE_SINGLETON(Exec_DefinitionRegistry);

Exec_DefinitionRegistry::Exec_DefinitionRegistry()
    : _registrationBarrier(std::make_unique<Exec_RegistrationBarrier>())
{
    // Ensure the type registry is initialized before the definition registry so
    // that computation registrations will be able to look up value types.
    ExecTypeRegistry::GetInstance();

    // Populate the registry with builtin computation definitions.
    _RegisterBuiltinComputations();

    // Calling SetInstanceConstructed() makes it possible to call
    // TfSingleton<>::GetInstance() before this constructor has finished.
    //
    // This is neccessary because the following call to SubscribeTo() will
    // _immediately_ invoke all registry functions which will, in turn, most
    // likely call TfSingleton<>::GetInstance().
    TfSingleton<Exec_DefinitionRegistry>::SetInstanceConstructed(*this);

    // Now initialize the registry.
    //
    // We use ExecDefinitionRegistryTag to identify registry functions, rather
    // than the definition registry type, so Exec_DefinitionRegistry can remain
    // private.
    TfRegistryManager::GetInstance().SubscribeTo<ExecDefinitionRegistryTag>();

    // Callers of Exec_DefinitionRegistry::GetInstance() can now safely return
    // a fully-constructed registry.
    _registrationBarrier->SetFullyConstructed();
}

// This must be defined in the cpp file, or we get undefined symbols when
// linking.
// 
const Exec_DefinitionRegistry&
Exec_DefinitionRegistry::GetInstance()
{
    Exec_DefinitionRegistry &instance =
        TfSingleton<Exec_DefinitionRegistry>::GetInstance();
    instance._registrationBarrier->WaitUntilFullyConstructed();
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

    // Get the composed prim definition, creating it if necesseary, and use it
    // to look up the computation, or to determine that the requested
    // computation isn't defined for this prim.
    auto composedDefIt = _composedPrimDefinitions.find(schemaType);
    if (composedDefIt == _composedPrimDefinitions.end()) {
        // Note that we allow concurrent callers to race to compose prim
        // definitions, since it is safe to do so and we don't expect it to
        // happen in the common case.
        _ComposedPrimDefinition primDef =
            _ComposePrimDefinition(schemaType);

        composedDefIt = _composedPrimDefinitions.emplace(
            schemaType, std::move(primDef)).first;
    }

    const auto &compDefs = composedDefIt->second.primComputationDefinitions;
    const auto it = compDefs.find(computationName);
    return it == compDefs.end() ? nullptr : it->second;
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

Exec_DefinitionRegistry::_ComposedPrimDefinition
Exec_DefinitionRegistry::_ComposePrimDefinition(
    const TfType schemaType) const
{
    TRACE_FUNCTION();

    // Iterate over all ancestor types of the provider's schema type, from
    // derived to base, starting with the schema type itself. Ensure that plugin
    // computations have been loaded for each schema type for which they are
    // registered. Add all plugin computations registered for each type to the
    // composed prim definition.
    //
    // TODO: Add support for computations that are registered for applied
    // schemas. To do that, instead of keying off the schema type we will need
    // to use a "configuration key" that combines the typed schema with applied
    // schemas. We will also need to search through all applied schemas, in
    // strength order, in addition to searching up the typed schema type
    // hierarchy.

    std::vector<TfType> schemaAncestorTypes;
    schemaType.GetAllAncestorTypes(&schemaAncestorTypes);

    // Build up the composed prim definition.
    _ComposedPrimDefinition primDef;

    for (const TfType type : schemaAncestorTypes) {

        // TODO: For all but the first type, it makes sense to look in
        // _composedPrimDefinitions to see if we have already composed the base
        // type, and then to merge, rather than keep searching up the type
        // hierarchy.

        if (const auto pluginIt = _pluginPrimComputationDefinitions.find(type);
            pluginIt != _pluginPrimComputationDefinitions.end()) {
            for (const Exec_PluginComputationDefinition &computationDef :
                     pluginIt->second) {
                primDef.primComputationDefinitions.emplace(
                    computationDef.GetComputationName(),
                    &computationDef);
            }
        }
    }

    return primDef;
}

void
Exec_DefinitionRegistry::_RegisterPrimComputation(
    TfType schemaType,
    const TfToken &computationName,
    TfType resultType,
    ExecCallbackFn &&callback,
    Exec_InputKeyVectorRefPtr &&inputKeys)
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
        _pluginPrimComputationDefinitions[schemaType].emplace(
            resultType,
            computationName,
            std::move(callback),
            std::move(inputKeys)).second;

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

PXR_NAMESPACE_CLOSE_SCOPE
