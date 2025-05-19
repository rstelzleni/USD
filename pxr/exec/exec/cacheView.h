//
// Copyright 2025 Pixar
//
// Licensed under the terms set forth in the LICENSE.txt file available at
// https://openusd.org/license.
//
#ifndef PXR_EXEC_EXEC_CACHE_VIEW_H
#define PXR_EXEC_EXEC_CACHE_VIEW_H

#include "pxr/pxr.h"

#include "pxr/exec/exec/api.h"

#include "pxr/base/tf/span.h"
#include "pxr/exec/vdf/dataManagerFacade.h"

#include <optional>

PXR_NAMESPACE_OPEN_SCOPE

class VdfMaskedOutput;
class VtValue;

/// A view into values cached by ExecSystem.
///
/// This class is not intended to be used directly by users but as part of
/// higher level libraries.  Cache views must not outlive the ExecSystem or
/// request from which they were built.
///
class Exec_CacheView
{
public:
    /// Constructs an invalid cache view.
    Exec_CacheView() = default;

    /// Returns \c true if \p idx is evaluated and stores the computed value
    /// in \p *result.
    ///
    /// Otherwise, returns \c false.
    ///
    EXEC_API
    bool Extract(int idx, VtValue *result) const;

private:
    friend class Exec_RequestImpl;
    Exec_CacheView(
        const VdfDataManagerFacade dataManager,
        TfSpan<const VdfMaskedOutput> outputs)
        : _dataManager(dataManager)
        , _outputs(outputs)
    {}

private:
    std::optional<const VdfDataManagerFacade> _dataManager;
    const TfSpan<const VdfMaskedOutput> _outputs{};
};

PXR_NAMESPACE_CLOSE_SCOPE

#endif
