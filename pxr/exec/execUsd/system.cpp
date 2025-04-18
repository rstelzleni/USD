//
// Copyright 2025 Pixar
//
// Licensed under the terms set forth in the LICENSE.txt file available at
// https://openusd.org/license.
//
#include "pxr/exec/execUsd/system.h"

#include "pxr/exec/execUsd/cacheView.h"
#include "pxr/exec/execUsd/request.h"
#include "pxr/exec/execUsd/requestImpl.h"
#include "pxr/exec/execUsd/sceneAdapter.h"

#include "pxr/exec/exec/valueKey.h"

#include "pxr/base/trace/trace.h"

PXR_NAMESPACE_OPEN_SCOPE

ExecUsdSystem::ExecUsdSystem(const UsdStageConstRefPtr &stage)
    : ExecSystem(ExecUsdSceneAdapter::AdaptStage(stage))
{
}

ExecUsdSystem::~ExecUsdSystem() = default;

ExecUsdRequest
ExecUsdSystem::BuildRequest(std::vector<ExecUsdValueKey> &&valueKeys)
{
    TRACE_FUNCTION();

    auto impl = std::make_shared<ExecUsd_RequestImpl>(std::move(valueKeys));
    _InsertRequest(impl);
    return ExecUsdRequest(std::move(impl));
}

void
ExecUsdSystem::PrepareRequest(const ExecUsdRequest &request)
{
    std::shared_ptr<ExecUsd_RequestImpl> requestImpl = request._GetImpl();
    if (!requestImpl) {
        TF_CODING_ERROR("Cannot prepare an expired request");
        return;
    }

    requestImpl->Compile(this);
    requestImpl->Schedule();
}

ExecUsdCacheView
ExecUsdSystem::CacheValues(const ExecUsdRequest &request)
{
    return ExecUsdCacheView();
}

PXR_NAMESPACE_CLOSE_SCOPE
