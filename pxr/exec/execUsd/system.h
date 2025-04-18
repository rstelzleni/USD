//
// Copyright 2025 Pixar
//
// Licensed under the terms set forth in the LICENSE.txt file available at
// https://openusd.org/license.
//
#ifndef PXR_EXEC_EXEC_USD_SYSTEM_H
#define PXR_EXEC_EXEC_USD_SYSTEM_H

/// \file

#include "pxr/pxr.h"

#include "pxr/exec/execUsd/api.h"

#include "pxr/base/tf/declarePtrs.h"
#include "pxr/exec/exec/system.h"

#include <vector>

PXR_NAMESPACE_OPEN_SCOPE

TF_DECLARE_REF_PTRS(UsdStage);

class ExecUsdCacheView;
class ExecUsdRequest;
class ExecUsdValueKey;

class ExecUsdSystem
    : public ExecSystem
{
public:
    EXECUSD_API
    explicit ExecUsdSystem(const UsdStageConstRefPtr &stage);

    // Systems are non-copyable and non-movable to simplify management of
    // back-pointers.
    ExecUsdSystem(const ExecUsdSystem &) = delete;
    ExecUsdSystem& operator=(const ExecUsdSystem &) = delete;

    EXECUSD_API
    ~ExecUsdSystem();

    EXECUSD_API
    ExecUsdRequest BuildRequest(std::vector<ExecUsdValueKey> &&valueKeys);

    EXECUSD_API
    void PrepareRequest(const ExecUsdRequest &request);

    EXECUSD_API
    ExecUsdCacheView CacheValues(const ExecUsdRequest &request);
};

PXR_NAMESPACE_CLOSE_SCOPE

#endif
