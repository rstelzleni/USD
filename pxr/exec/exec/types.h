//
// Copyright 2025 Pixar
//
// Licensed under the terms set forth in the LICENSE.txt file available at
// https://openusd.org/license.
//
#ifndef PXR_EXEC_EXEC_TYPES_H
#define PXR_EXEC_EXEC_TYPES_H

/// \file
///
/// This file contains definitions for trvial types, including type aliases, so
/// that source files that require these types can get access to them without
/// transitively including many headers, and otherwise pulling in unneeded
/// definitions. Defining these types in this public header also helps keep
/// other headers private to this library.
/// 

#include "pxr/pxr.h"

#include <functional>

PXR_NAMESPACE_OPEN_SCOPE

class VdfContext;

/// Function type used for computation callbacks.
using ExecCallbackFn = std::function<void (const VdfContext &context)>;

/// Type used to identify Exec_DefinitionRegistry registry functions.
///
/// We use a separate public type as the tag, rather than
/// Exec_DefinitionRegistry itself, to allow that type to remain private.
struct ExecDefinitionRegistryTag {};

PXR_NAMESPACE_CLOSE_SCOPE

#endif
