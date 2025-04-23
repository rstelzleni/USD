//
// Copyright 2025 Pixar
//
// Licensed under the terms set forth in the LICENSE.txt file available at
// https://openusd.org/license.
//
#include "pxr/exec/exec/system.h"

#include "pxr/exec/exec/compiler.h"
#include "pxr/exec/exec/program.h"
#include "pxr/exec/exec/requestImpl.h"

#include "pxr/exec/ef/executor.h"
#include "pxr/exec/vdf/parallelDataManagerVector.h"
#include "pxr/exec/vdf/parallelExecutorEngine.h"
#include "pxr/exec/vdf/parallelSpeculationExecutorEngine.h"

#include "pxr/base/tf/span.h"
#include "pxr/base/trace/trace.h"

#include <utility>

PXR_NAMESPACE_OPEN_SCOPE

ExecSystem::ExecSystem(EsfStage &&stage)
    : _stage(std::move(stage))
    , _program(std::make_unique<Exec_Program>())
{
    _CreateExecutor();
}

ExecSystem::~ExecSystem() = default;

void
ExecSystem::_InsertRequest(std::shared_ptr<Exec_RequestImpl> &&impl)
{
    _requests.push_back(std::move(impl));
}

std::vector<VdfMaskedOutput>
ExecSystem::_Compile(TfSpan<const ExecValueKey> valueKeys)
{
    Exec_Compiler compiler(_stage, _program.get());
    return compiler.Compile(valueKeys);
}

void
ExecSystem::_CreateExecutor()
{
    _executor = std::make_unique<
        EfExecutor<VdfParallelExecutorEngine, VdfParallelDataManagerVector>>();
}

void
ExecSystem::_InvalidateAll()
{
    TRACE_FUNCTION();

    _program = std::make_unique<Exec_Program>();
    _CreateExecutor();
    _requests.clear();
}

PXR_NAMESPACE_CLOSE_SCOPE
