//
// Copyright 2025 Pixar
//
// Licensed under the terms set forth in the LICENSE.txt file available at
// https://openusd.org/license.
//
#ifndef PXR_EXEC_EXEC_USD_REQUEST_IMPL_H
#define PXR_EXEC_EXEC_USD_REQUEST_IMPL_H

#include "pxr/pxr.h"

#include "pxr/exec/execUsd/api.h"

#include "pxr/exec/exec/requestImpl.h"

#include <vector>

PXR_NAMESPACE_OPEN_SCOPE

class ExecUsdSystem;
class ExecUsdValueKey;
class ExecValueKey;

// Contains Usd-specific data structures necessary to implement requests.
//
class ExecUsd_RequestImpl final
    : public Exec_RequestImpl
{
    ExecUsd_RequestImpl(const ExecUsd_RequestImpl&) = delete;
    ExecUsd_RequestImpl& operator=(const ExecUsd_RequestImpl&) = delete;

public:
    explicit ExecUsd_RequestImpl(std::vector<ExecUsdValueKey> &&valueKeys);
    ~ExecUsd_RequestImpl();

    void Compile(ExecUsdSystem *system);
    void Schedule();

private:
    std::vector<ExecUsdValueKey> _valueKeys;
};

PXR_NAMESPACE_CLOSE_SCOPE

#endif
