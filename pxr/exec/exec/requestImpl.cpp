//
// Copyright 2025 Pixar
//
// Licensed under the terms set forth in the LICENSE.txt file available at
// https://openusd.org/license.
//
#include "pxr/exec/exec/requestImpl.h"

#include "pxr/exec/exec/system.h"
#include "pxr/exec/exec/typeRegistry.h"
#include "pxr/exec/exec/valueKey.h"

#include "pxr/exec/vdf/request.h"
#include "pxr/exec/vdf/schedule.h"
#include "pxr/exec/vdf/scheduler.h"

#include "pxr/base/tf/span.h"

PXR_NAMESPACE_OPEN_SCOPE

Exec_RequestImpl::Exec_RequestImpl() = default;

Exec_RequestImpl::~Exec_RequestImpl() = default;

void
Exec_RequestImpl::_Compile(
    ExecSystem *const system,
    TfSpan<const ExecValueKey> valueKeys)
{
    if (!TF_VERIFY(system)) {
        return;
    }

    _leafOutputs = system->_Compile(valueKeys);
    if (!TF_VERIFY(_leafOutputs.size() == valueKeys.size())) {
        // If we somehow got the wrong number of outputs from compilation, we
        // have no idea if the indices correspond correctly so zero out all
        // the outputs.
        _leafOutputs.assign(valueKeys.size(), VdfMaskedOutput());
    }
}

void
Exec_RequestImpl::_Schedule()
{
    // All outputs received from compilation are expected to be valid.  If
    // they are not, an error should have already been issued.
    std::vector<VdfMaskedOutput> outputs;
    outputs.reserve(_leafOutputs.size());
    for (const VdfMaskedOutput &mo : _leafOutputs) {
        if (mo) {
            outputs.push_back(mo);
        }
    }
    _computeRequest = std::make_unique<VdfRequest>(std::move(outputs));

    _schedule = std::make_unique<VdfSchedule>();
    VdfScheduler::Schedule(
        *_computeRequest, _schedule.get(), /* topologicallySort */ false);
}

void
Exec_RequestImpl::_CacheValues(ExecSystem *const system)
{
    if (!TF_VERIFY(system)) {
        return;
    }

    system->_CacheValues(*_schedule, *_computeRequest);
}

PXR_NAMESPACE_CLOSE_SCOPE
