//
// Copyright 2025 Pixar
//
// Licensed under the terms set forth in the LICENSE.txt file available at
// https://openusd.org/license.
//
#ifndef PXR_EXEC_EXEC_REQUEST_IMPL_H
#define PXR_EXEC_EXEC_REQUEST_IMPL_H

#include "pxr/pxr.h"

#include "pxr/exec/exec/api.h"
#include "pxr/exec/exec/request.h"
#include "pxr/exec/exec/types.h"

#include "pxr/base/tf/bits.h"
#include "pxr/exec/ef/timeInterval.h"
#include "pxr/exec/vdf/maskedOutput.h"
#include "pxr/exec/vdf/types.h"

#include <memory>
#include <unordered_map>
#include <vector>

PXR_NAMESPACE_OPEN_SCOPE

class Exec_CacheView;
class ExecSystem;
class ExecValueKey;
class TfBits;
template <typename> class TfSpan;
class VdfRequest;
class VdfSchedule;

/// Contains data structures necessary to implement exec requests that are
/// independent of scene description.
///
/// Concrete implementations inherit from Exec_RequestImpl to implement any
/// functionality that is specific to the scene description system.
///
class EXEC_API_TYPE Exec_RequestImpl
{
public:
    /// Notify the request of invalid computed values as a consequence of
    /// authored value, or structural invalidation.
    /// 
    /// Call site provides the set of \p invalidLeafNodes, along with a
    /// combined \p invalidInterval denoting the time interval over which leaf
    /// nodes are invalid.
    /// 
    /// Additionally, a span of \p invalidProperties may be provided. The
    /// entries in the span contain the scene description property path along
    /// with the invalid time range for said property. The \p compiledProperties
    /// bit set specifies all properties in the \p invalidProperties span, which
    /// are compiled through exec. All other invalid properties only manifest in
    /// scene description.
    /// 
    void DidInvalidateComputedValues(
        const std::vector<const VdfNode *> &invalidLeafNodes,
        const EfTimeInterval &invalidInterval,
        TfSpan<ExecInvalidAuthoredValue> invalidProperties,
        const TfBits &compiledProperties);

protected:
    EXEC_API
    explicit Exec_RequestImpl(
        const ExecRequestIndexedInvalidationCallback &invalidationCallback);

    Exec_RequestImpl(const Exec_RequestImpl&) = delete;
    Exec_RequestImpl& operator=(const Exec_RequestImpl&) = delete;

    EXEC_API
    virtual ~Exec_RequestImpl();

    /// Compiles outputs for the value keys in the request.
    EXEC_API
    void _Compile(ExecSystem *system, TfSpan<const ExecValueKey> valueKeys);

    /// Builds the schedule for the request.
    EXEC_API
    void _Schedule();

    /// Computes the value keys in the request.
    EXEC_API
    Exec_CacheView _CacheValues(ExecSystem *system);

private:
    // The compiled leaf output.
    std::vector<VdfMaskedOutput> _leafOutputs;

    // Maps leaf outputs to their index in the array of valueKeys the request
    // was built with.
    std::unordered_map<VdfMaskedOutput, size_t, VdfMaskedOutput::Hash>
        _leafOutputToIndex;

    // The compute request to cache values with.
    std::unique_ptr<VdfRequest> _computeRequest;

    // The schedule to cache values with.
    std::unique_ptr<VdfSchedule> _schedule;

    // The combined time interval for which invalidation has been sent out.
    EfTimeInterval _lastInvalidatedInterval;

    // The combined set of indices for which invalidation has been sent out.
    TfBits _lastInvalidatedIndices;

    // The invalidation callback to invoke when computed values change.
    ExecRequestIndexedInvalidationCallback _invalidationCallback;
};

PXR_NAMESPACE_CLOSE_SCOPE

#endif
