//
// Copyright 2025 Pixar
//
// Licensed under the terms set forth in the LICENSE.txt file available at
// https://openusd.org/license.
//
#ifndef PXR_EXEC_EXEC_USD_CACHE_VIEW_H
#define PXR_EXEC_EXEC_USD_CACHE_VIEW_H

/// \file

#include "pxr/pxr.h"

#include "pxr/exec/execUsd/api.h"

#include "pxr/exec/exec/cacheView.h"

#include <utility>

PXR_NAMESPACE_OPEN_SCOPE

/// Provides a view of values computed by ExecUsdSystem::CacheValues.
///
/// Cache views must not outlive the ExecUsdSystem or ExecUsdRequest from
/// which they were built.
///
class ExecUsdCacheView
{
public:
    /// Construct an invalid view.
    ///
    ExecUsdCacheView() = default;

    /// Returns \c true if \p idx is evaluated and stores the computed value
    /// in \p *result.
    ///
    /// Otherwise, returns \c false.
    ///
    EXECUSD_API
    bool Extract(int idx, VtValue *result) const;

private:
    friend class ExecUsdSystem;
    explicit ExecUsdCacheView(
        Exec_CacheView &&view)
        : _view(std::move(view))
    {}

private:
    Exec_CacheView _view;
};

PXR_NAMESPACE_CLOSE_SCOPE

#endif
