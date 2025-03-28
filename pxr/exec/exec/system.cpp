//
// Copyright 2025 Pixar
//
// Licensed under the terms set forth in the LICENSE.txt file available at
// https://openusd.org/license.
//
#include "pxr/exec/exec/system.h"

#include "pxr/exec/exec/compiledOutputCache.h"
#include "pxr/exec/exec/compiler.h"
#include "pxr/exec/exec/request.h"
#include "pxr/exec/exec/requestImpl.h"

#include "pxr/exec/ef/executor.h"
#include "pxr/exec/ef/timeInputNode.h"
#include "pxr/exec/vdf/grapher.h"
#include "pxr/exec/vdf/network.h"
#include "pxr/exec/vdf/parallelDataManagerVector.h"
#include "pxr/exec/vdf/parallelExecutorEngine.h"
#include "pxr/exec/vdf/parallelSpeculationExecutorEngine.h"

#include "pxr/base/tf/span.h"

#include <utility>

PXR_NAMESPACE_OPEN_SCOPE

ExecSystem::ExecSystem(EsfStage &&stage) :
    _stage(std::move(stage)),
    _compiledOutputCache(std::make_unique<Exec_CompiledOutputCache>()),
    _network(std::make_unique<VdfNetwork>()),
    _timeInput(new EfTimeInputNode(_network.get())),
    _executor(std::make_unique<EfExecutor<
                  VdfParallelExecutorEngine, VdfParallelDataManagerVector>>())
{
}

ExecSystem::~ExecSystem() = default;

ExecRequest
ExecSystem::BuildRequest(std::vector<ExecValueKey> &&valueKeys)
{
    const std::shared_ptr<Exec_RequestImpl> &impl = *_requests.push_back(
        std::make_shared<Exec_RequestImpl>(std::move(valueKeys)));
    return ExecRequest(impl);
}

void
ExecSystem::PrepareRequest(const ExecRequest &request)
{
    if (std::shared_ptr<Exec_RequestImpl> impl = request._impl.lock()) {
        impl->Compile(this);
        impl->Schedule();
    }
}

void
ExecSystem::GraphNetwork(const char *filename) const
{
    VdfGrapher::GraphToFile(*_network, filename);
}

std::vector<VdfMaskedOutput>
ExecSystem::_Compile(TfSpan<const ExecValueKey> valueKeys)
{
    Exec_Compiler compiler(_stage, _compiledOutputCache.get(), _network.get());
    return compiler.Compile(valueKeys);
}

PXR_NAMESPACE_CLOSE_SCOPE
