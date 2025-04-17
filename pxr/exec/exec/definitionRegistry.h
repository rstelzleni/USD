//
// Copyright 2025 Pixar
//
// Licensed under the terms set forth in the LICENSE.txt file available at
// https://openusd.org/license.
//
#ifndef PXR_EXEC_EXEC_DEFINITION_REGISTRY_H
#define PXR_EXEC_EXEC_DEFINITION_REGISTRY_H

#include "pxr/pxr.h"

#include "pxr/exec/exec/api.h"
#include "pxr/exec/exec/computationDefinition.h"
#include "pxr/exec/exec/inputKey.h"
#include "pxr/exec/exec/types.h"

#include "pxr/base/tf/hash.h"
#include "pxr/base/tf/singleton.h"
#include "pxr/base/tf/token.h"
#include "pxr/base/tf/type.h"
#include "pxr/base/tf/weakBase.h"

#include <unordered_map>
#include <tuple>

PXR_NAMESPACE_OPEN_SCOPE

/// Singleton that stores computation definitions registered for schemas that
/// define computations.
///
class Exec_DefinitionRegistry : public TfWeakBase
{
public:
    Exec_DefinitionRegistry(
        const Exec_DefinitionRegistry &) = delete;
    Exec_DefinitionRegistry &operator=(
        const Exec_DefinitionRegistry &) = delete;

    /// Provides access to the singleton instance, first ensuring it is
    /// constructed.
    EXEC_API
    static Exec_DefinitionRegistry& GetInstance();

    /// Returns the definition for the prim computation named
    /// \p computationName registered for schema \p schemaType.
    EXEC_API
    const Exec_ComputationDefinition *GetPrimComputationDefinition(
        TfType schemaType,
        const TfToken &computationName) const;

    /// Returns the definition for the attribute computation named
    /// \p computationName registered for schema \p schemaType for attributes
    /// named \p attributeName.
    const Exec_ComputationDefinition *GetAttributeComputationDefinition(
        TfType primSchemaType,
        const TfToken &attributeName,
        const TfToken &computationName) const;

    // Only computation builders can register plugin computations.
    class RegisterPluginComputationAccess
    {
        friend class Exec_PrimComputationBuilder;

        // Registers a prim computation for \p schemaType.
        inline static void _RegisterPrimComputation(
            TfType schemaType,
            const TfToken &computationName,
            TfType resultType,
            ExecCallbackFn &&callback,
            Exec_InputKeyVector &&inputKeys);
    };

    class RegisterBuiltinComputationAccess
    {
        friend class Exec_BuiltinComputations;

        // Registers a builtin computation named \p computationName.
        static void _Register(
            const TfToken &computationName,
            Exec_ComputationDefinition *definition) {
            GetInstance().
                _RegisterBuiltinComputation(computationName, definition);
        }
    };

private:

    // Only TfSingleton can create instances.
    friend class TfSingleton<Exec_DefinitionRegistry>;

    Exec_DefinitionRegistry();

    void _RegisterPrimComputation(
        TfType schemaType,
        const TfToken &computationName,
        TfType resultType,
        ExecCallbackFn &&callback,
        Exec_InputKeyVector &&inputKeys);

    void _RegisterBuiltinComputation(
        const TfToken &computationName,
        Exec_ComputationDefinition *definition);

private:

    // Map from (schemaType, computationName) to plugin prim computation
    // definition.
    std::unordered_map<
        std::tuple<TfType, TfToken>,
        Exec_PluginComputationDefinition,
        TfHash>
        _pluginPrimComputationDefinitions;

    // Map from computationName to builtin computation
    // definition.
    std::unordered_map<
        TfToken,
        std::unique_ptr<Exec_ComputationDefinition>,
        TfHash>
        _builtinComputationDefinitions;
};

void
Exec_DefinitionRegistry::RegisterPluginComputationAccess::
_RegisterPrimComputation(
    TfType schemaType,
    const TfToken &computationName,
    TfType resultType,
    ExecCallbackFn &&callback,
    Exec_InputKeyVector &&inputKeys)
{
    GetInstance()._RegisterPrimComputation(
        schemaType,
        computationName,
        resultType,
        std::move(callback),
        std::move(inputKeys));
}

PXR_NAMESPACE_CLOSE_SCOPE

#endif
