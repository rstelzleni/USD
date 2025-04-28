//
// Copyright 2025 Pixar
//
// Licensed under the terms set forth in the LICENSE.txt file available at
// https://openusd.org/license.
//
#ifndef PXR_EXEC_EXEC_USD_PROPERTY_H
#define PXR_EXEC_EXEC_USD_PROPERTY_H

#include "pxr/pxr.h"

#include "pxr/exec/execUsd/object.h"

#include "pxr/exec/esf/property.h"
#include "pxr/usd/usd/property.h"

#include <type_traits>

PXR_NAMESPACE_OPEN_SCOPE

/// Common implementation of EsfPropertyInterface.
///
/// This implementation wraps an instance of UsdProperty or subclass of
/// UsdProperty. The exact type is specified by the \p UsdPropertyType template
/// parameter.
///
/// This class inherits from ExecUsd_ObjectImpl, which itself inherits from the
/// \p InterfaceType template parameter. This type must be EsfPropertyInterface,
/// or any other interface that extends EsfPropertyInterface.
///
template <class InterfaceType, class UsdPropertyType>
class ExecUsd_PropertyImpl
    : public ExecUsd_ObjectImpl<InterfaceType, UsdPropertyType>
{
    static_assert(std::is_base_of_v<EsfPropertyInterface, InterfaceType>);
    static_assert(std::is_base_of_v<UsdProperty, UsdPropertyType>);

public:
    ~ExecUsd_PropertyImpl() override;

    /// Copies the provided property into this instance.
    ExecUsd_PropertyImpl(const UsdPropertyType &property)
        : ExecUsd_ObjectImpl<InterfaceType, UsdPropertyType>(property) {}

    /// Moves the provided property into this instance.
    ExecUsd_PropertyImpl(UsdPropertyType &&property)
        : ExecUsd_ObjectImpl<InterfaceType, UsdPropertyType>(
            std::move(property)) {}

private:
    // EsfPropertyInterface implementation.
    TfToken _GetBaseName() const final;
    TfToken _GetNamespace() const final;
};

/// Implementation of EsfPropertyInterface that wraps a UsdProperty.
using ExecUsd_Property =
    ExecUsd_PropertyImpl<EsfPropertyInterface, UsdProperty>;

PXR_NAMESPACE_CLOSE_SCOPE

#endif
