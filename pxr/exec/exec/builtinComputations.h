//
// Copyright 2025 Pixar
//
// Licensed under the terms set forth in the LICENSE.txt file available at
// https://openusd.org/license.
//
#ifndef PXR_EXEC_EXEC_BUILTIN_COMPUTATIONS_H
#define PXR_EXEC_EXEC_BUILTIN_COMPUTATIONS_H

#include "pxr/pxr.h"

#include "pxr/exec/exec/api.h"

#include "pxr/base/tf/staticData.h"
#include "pxr/base/tf/token.h"

#include <vector>

PXR_NAMESPACE_OPEN_SCOPE

/// Tokens representing the built-in computations available on various provider
/// types.
///
/// \defgroup group_Exec_Builtin_Computations Builtin Exec Computations
///
/// These tokens can all be used in [input
/// registrations](#group_Exec_InputRegistrations) to request input values for
/// plugin computations. They can also be passed to compute APIs to request
/// computed values.
///
/// These computation tokens are publicly accessible by dereferencing the
/// `ExecBuiltinComputationTokens` static data.
///
struct Exec_BuiltinComputations
{
    /// \defgroup group_Mf_ExecBuiltinComputations_Stage Stage Computations
    /// 
    /// Builtin computations for computing information about the provider's
    /// stage.
    /// 
    /// \ingroup group_Exec_Builtin_Computations
    /// @{

    /// Computes the current time on the stage.
    ///
    /// \returns an EfTime value.
    ///
    /// \note
    /// The computation provider must be the stage.
    ///
    /// # Example
    /// 
    /// ```{.cpp}
    /// self.PrimComputation(_tokens->myComputation)
    ///     .Callback<EfTime>( /* . . . */ )
    ///     .Inputs(
    ///         Stage()
    ///             .Computation<EfTime>(ExecBuiltinComputations->computeTime)
    ///     );
    /// ```
    ///
    /// \hideinitializer
    const TfToken computeTime =
        _RegisterBuiltin("computeTime");

    /// @}


    /// \defgroup group_Exec_Attribute_Comptuations Attribute Computations
    /// 
    /// Builtin computations for computing information about attributes.
    /// 
    /// \ingroup group_Exec_Builtin_Computations
    /// @{

    /// Computes the provider attribute's value.
    ///
    /// \returns a value whose type is the provider attribute's scalar value
    /// type.
    ///
    /// \note
    /// The computation provider must be an attribute.
    ///
    /// # Example
    /// 
    /// ```{.cpp}
    /// self.PrimComputation(_tokens->myComputation)
    ///     .Callback<double>( /* . . . */ )
    ///     .Inputs(
    ///         Attribute(_tokens->myAttribute)
    ///             .Computation<double>(ExecBuiltinComputations->computeValue)
    ///             .Required()
    ///     );
    /// ```
    ///
    /// \hideinitializer
    const TfToken computeValue =
        _RegisterBuiltin("computeValue");

    /// @}

    /// Returns all builtin computation tokens.
    const std::vector<TfToken> &GetComputationTokens();

private:

    // Returns a token that is the given name string with a double-underscore
    // prefix, to be used as the computation token for a built-in with the given
    // name. This also registers the builtin in the vector returned by
    // GetComputations.
    EXEC_API
    TfToken _RegisterBuiltin(const std::string &name);

    static constexpr char _computationNamePrefix[] = "__";
};

// Used to publicly access builtin computation tokens.
EXEC_API
extern TfStaticData<Exec_BuiltinComputations> ExecBuiltinComputations;

PXR_NAMESPACE_CLOSE_SCOPE

#endif
