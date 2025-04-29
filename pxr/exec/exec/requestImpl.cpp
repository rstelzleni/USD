//
// Copyright 2025 Pixar
//
// Licensed under the terms set forth in the LICENSE.txt file available at
// https://openusd.org/license.
//
#include "pxr/exec/exec/requestImpl.h"

#include "pxr/exec/exec/cacheView.h"
#include "pxr/exec/exec/system.h"
#include "pxr/exec/exec/typeRegistry.h"
#include "pxr/exec/exec/types.h"
#include "pxr/exec/exec/valueKey.h"

#include "pxr/base/tf/bits.h"
#include "pxr/base/tf/span.h"
#include "pxr/base/trace/trace.h"
#include "pxr/exec/ef/leafNode.h"
#include "pxr/exec/vdf/request.h"
#include "pxr/exec/vdf/schedule.h"
#include "pxr/exec/vdf/scheduler.h"
#include "pxr/exec/vdf/types.h"

PXR_NAMESPACE_OPEN_SCOPE

Exec_RequestImpl::Exec_RequestImpl(
    const ExecRequestIndexedInvalidationCallback &invalidationCallback)
    : _lastInvalidatedInterval(EfTimeInterval::GetFullInterval())
    , _invalidationCallback(invalidationCallback)
{}

Exec_RequestImpl::~Exec_RequestImpl() = default;

void 
Exec_RequestImpl::DidInvalidateComputedValues(
    const std::vector<const VdfNode *> &invalidLeafNodes,
    const EfTimeInterval &invalidInterval,
    TfSpan<ExecInvalidAuthoredValue> invalidProperties,
    const TfBits &compiledProperties)
{
    if (!_invalidationCallback || _leafOutputs.empty()) {
        return;
    }

    TRACE_FUNCTION();

    // Invalid leaf outputs will need to be converted into indices for client
    // notification. Here, we build a data structure for efficient lookup.
    if (_leafOutputToIndex.size() != _leafOutputs.size()) {
        TRACE_FUNCTION_SCOPE("rebuilding output-to-index map");
        _leafOutputToIndex.clear();
        _leafOutputToIndex.reserve(_leafOutputs.size());
        for (size_t i = 0; i < _leafOutputs.size(); ++i) {
            _leafOutputToIndex.emplace(_leafOutputs[i], i);
        }
    }

    // This is considered new invalidation only if the invalidation interval
    // isn't already fully contained in the last invalidation interval.
    const bool isNewlyInvalidInterval = invalidInterval.IsFullInterval()
        ? !_lastInvalidatedInterval.IsFullInterval()
        : !_lastInvalidatedInterval.Contains(invalidInterval);
    if (isNewlyInvalidInterval) {
        _lastInvalidatedInterval |= invalidInterval;
    }

    // Build a set of invalid indices from the provided invalid leaf nodes.
    ExecRequestIndexSet invalidIndices;
    for (const VdfNode *const leafNode : invalidLeafNodes) {
        // Every invalid leaf node should still be connected to a source output.
        const VdfMaskedOutput mo = EfLeafNode::GetSourceMaskedOutput(*leafNode);
        if (!TF_VERIFY(mo)) {
            continue;
        }

        // All requests are notified about all computed value invalidation, but
        // not all the invalid leaf nodes may be included in this particular
        // request.
        const auto it = _leafOutputToIndex.find(mo);
        if (it == _leafOutputToIndex.end()) {
            continue;
        }

        // Determine if invalidation has already been sent out for the invalid
        // index. If not, record this index as being invalid.
        const size_t index = it->second;
        if (isNewlyInvalidInterval || !_lastInvalidatedIndices.IsSet(index)) {
            invalidIndices.insert(index);
        }
        _lastInvalidatedIndices.Set(index); 
    }

    // TODO: Handle invalid properties which are not computed through exec.
    // In doing so we must dispatch to the derived class in order to let the
    // specific scene description library determine properties, which do not
    // require execution.

    // Only invoke the invalidation callback if there are any invalid indices
    // from this request.
    if (!invalidIndices.empty()) {
        TRACE_FUNCTION_SCOPE("invalidation callback");
        _invalidationCallback(invalidIndices, invalidInterval);
    }
}

void
Exec_RequestImpl::_Compile(
    ExecSystem *const system,
    TfSpan<const ExecValueKey> valueKeys)
{
    if (!TF_VERIFY(system)) {
        return;
    }

    // If there is a schedule and it has not been invalidated it's safe to
    // assume that the network hasn't changed and that we don't need to compile.
    if (_schedule && _schedule->IsValid()) {
        return;
    }

    // Compile the value keys.
    _leafOutputs = system->_Compile(valueKeys);
    if (!TF_VERIFY(_leafOutputs.size() == valueKeys.size())) {
        // If we somehow got the wrong number of outputs from compilation, we
        // have no idea if the indices correspond correctly so zero out all
        // the outputs.
        _leafOutputs.assign(valueKeys.size(), VdfMaskedOutput());
    }

    // After compiling new outputs, we need to invalidate all data dependent on
    // the compiled network and the set of compiled leaf outputs.
    _leafOutputToIndex.clear();
    _computeRequest.reset();
    _schedule.reset();
    _lastInvalidatedIndices.Resize(_leafOutputs.size());
    _lastInvalidatedIndices.ClearAll();
}

void
Exec_RequestImpl::_Schedule()
{
    // The compute request only needs to be rebuilt if the compiled outputs
    // change.
    if (!_computeRequest) {
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
    }

    // We only need to schedule if there isn't already a valid schedule.
    if (!_schedule || !_schedule->IsValid()) {
        _schedule = std::make_unique<VdfSchedule>();
        VdfScheduler::Schedule(
            *_computeRequest, _schedule.get(), /* topologicallySort */ false);
    }
}

Exec_CacheView
Exec_RequestImpl::_CacheValues(ExecSystem *const system)
{
    if (!TF_VERIFY(system)) {
        return Exec_CacheView();
    }

    // Reset the last invalidation state so that new invalidation is properly
    // sent out as clients renew their interest in the computed values included
    // in this request.
    _lastInvalidatedIndices.ClearAll();
    _lastInvalidatedInterval.Clear();

    // Compute the values.
    system->_CacheValues(*_schedule, *_computeRequest);

    // Return an exec cache view for the computed values.
    return Exec_CacheView(system->_GetMainExecutor(), _leafOutputs);
}

PXR_NAMESPACE_CLOSE_SCOPE
