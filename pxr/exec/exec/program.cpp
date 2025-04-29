//
// Copyright 2025 Pixar
//
// Licensed under the terms set forth in the LICENSE.txt file available at
// https://openusd.org/license.
//
#include "pxr/exec/exec/program.h"

#include "pxr/base/tf/span.h"
#include "pxr/base/tf/token.h"
#include "pxr/base/trace/trace.h"
#include "pxr/exec/ef/timeInputNode.h"
#include "pxr/exec/ef/timeInterval.h"
#include "pxr/exec/vdf/executorInterface.h"
#include "pxr/exec/vdf/grapher.h"
#include "pxr/exec/vdf/maskedOutput.h"
#include "pxr/exec/vdf/node.h"
#include "pxr/exec/vdf/typedVector.h"
#include "pxr/usd/sdf/path.h"

PXR_NAMESPACE_OPEN_SCOPE

class Exec_Program::_EditMonitor final : public VdfNetwork::EditMonitor {
public:
    explicit _EditMonitor(EfLeafNodeCache *const leafNodeCache)
        : _leafNodeCache(leafNodeCache)
    {}

    void WillClear() override {
        _leafNodeCache->Clear();
    }

    void DidConnect(const VdfConnection *connection) override {
        _leafNodeCache->DidConnect(*connection);
    }

    void DidAddNode(const VdfNode *node) override {}

    void WillDelete(const VdfConnection *connection) override {
        _leafNodeCache->WillDeleteConnection(*connection);
    }

    void WillDelete(const VdfNode *node) override {}

private:
    EfLeafNodeCache *const _leafNodeCache;
};

Exec_Program::Exec_Program()
    : _timeInputNode(new EfTimeInputNode(&_network))
    , _editMonitor(std::make_unique<_EditMonitor>(&_leafNodeCache))
{
    _network.RegisterEditMonitor(_editMonitor.get());
}

Exec_Program::~Exec_Program()
{
    _network.UnregisterEditMonitor(_editMonitor.get());
}

void
Exec_Program::Connect(
    const EsfJournal &journal,
    const TfSpan<const VdfMaskedOutput> outputs,
    VdfNode *inputNode,
    const TfToken &inputName)
{
    for (const VdfMaskedOutput &output : outputs) {
        // XXX
        // Note that it's possible for the SourceOutputs contains null outputs.
        // This can happen if the input depends on output keys that could not
        // be compiled (e.g. requesting a computation on a prim which does not
        // have a registered computation of that name). This can be re-visited
        // if output keys contain Exec_ComputationDefinition pointers, as
        // that requires we find a matching computation in order to form that
        // output key.
        if (output) {
            _network.Connect(output, inputNode, inputName);
        }
    }
    _uncompilationTable.AddRulesForInput(
        inputNode->GetId(), inputName, journal);
}

std::tuple<const std::vector<const VdfNode *> &, TfBits, EfTimeInterval>
Exec_Program::InvalidateAuthoredValues(
    TfSpan<ExecInvalidAuthoredValue> invalidProperties)
{
    TRACE_FUNCTION();

    const size_t numInvalidProperties = invalidProperties.size();

    VdfMaskedOutputVector leafInvalidationRequest;
    leafInvalidationRequest.reserve(numInvalidProperties);
    TfBits compiledProperties(numInvalidProperties);
    _uninitializedInputNodes.reserve(numInvalidProperties);
    EfTimeInterval totalInvalidInterval;

    for (size_t i = 0; i < numInvalidProperties; ++i) {
        const auto &[path, interval] = invalidProperties[i];
        const auto it = _inputNodes.find(path);

        // Not every invalid property is also an input to the exec network.
        // If any of these properties have been included in an exec request,
        // clients still expect to receive invalidation notices, though.
        // However, we can skip including this property in the search for
        // dependent leaf nodes in that case.
        const bool isCompiled = it != _inputNodes.end();
        if (!isCompiled) {
            continue;
        }

        // Indicate this property was compiled.
        compiledProperties.Set(i);

        // Get the input node from the network.
        const VdfId nodeId = it->second;
        VdfNode *const node = _network.GetNodeById(nodeId);

        // We expect uncompiled input nodes to have been removed from the
        // _inputNodes array by now.
        if (!TF_VERIFY(node)) {
            continue;
        }

        // If this is an input node to the exec network, we need to make sure
        // that it is re-initialized before the next round of evaluation.
        _uninitializedInputNodes.push_back(nodeId);

        // Queue the input node's output(s) for leaf node invalidation.
        leafInvalidationRequest.emplace_back(
            node->GetOutput(), VdfMask::AllOnes(1));

        // Accumulate the invalid time interval.
        totalInvalidInterval |= interval;
    }

    // Find all the leaf nodes reachable from the input nodes.
    // We won't ask the leaf node cache to incur the cost of performing
    // incremental updates on the resulting cached traversal, because it is not
    // guaranteed that we will repeatedly see the exact same authored value
    // invalidation across rounds of structural change processing (in contrast
    // to time invalidation).
    const std::vector<const VdfNode *> &leafNodes = _leafNodeCache.FindNodes(
        leafInvalidationRequest, /* updateIncrementally = */ false);

    // TODO: Perform page cache invalidation.

    return {leafNodes, compiledProperties, totalInvalidInterval};
}

void
Exec_Program::InitializeTime(
    VdfExecutorInterface *const executor,
    const EfTime &newTime) const
{
    // Retrieve the old time from the executor data manager.
    const VdfVector *const oldTimeVector = executor->GetOutputValue(
        *_timeInputNode->GetOutput(), VdfMask::AllOnes(1));

    // If there isn't already a time value stored in the executor data manager,
    // perform first time initialization and return.
    if (!oldTimeVector) {
        executor->SetOutputValue(
            *_timeInputNode->GetOutput(),
            VdfTypedVector<EfTime>(newTime),
            VdfMask::AllOnes(1));
        return;
    }

    // Get the old time value from the vector. If there is not change in time,
    // we can return without performing invalidation.
    const EfTime oldTime = oldTimeVector->GetReadAccessor<EfTime>()[0];
    if (oldTime == newTime) {
        return;
    }

    TRACE_FUNCTION();

    // TODO: Collect time dependent properties and perform executor invalidation
    // after time changes.

    // Set the new time value on the executor.
    executor->SetOutputValue(
        *_timeInputNode->GetOutput(),
        VdfTypedVector<EfTime>(newTime),
        VdfMask::AllOnes(1));
}

VdfMaskedOutputVector
Exec_Program::InitializeInputNodes()
{
    if (_uninitializedInputNodes.empty()) {
        return {};
    }

    TRACE_FUNCTION();

    // Collect the invalid outputs for all invalid input nodes accumulated
    // through previous rounds of authored value invalidation.
    VdfMaskedOutputVector invalidationRequest;
    invalidationRequest.reserve(_uninitializedInputNodes.size());
    for (const VdfId nodeId : _uninitializedInputNodes) {
        VdfNode *const node = _network.GetNodeById(nodeId);

        // Some nodes may have been uncompiled since they were marked as being
        // uninitialized. It's okay to simply skip these nodes.
        if (!node) {
            continue;
        }

        invalidationRequest.emplace_back(
            node->GetOutput(), VdfMask::AllOnes(1));
    }

    _uninitializedInputNodes.clear();

    return invalidationRequest;
}

void
Exec_Program::GraphNetwork(const char *const filename) const
{
    VdfGrapher::GraphToFile(_network, filename);
}

void
Exec_Program::_AddNode(const EsfJournal &journal, const VdfNode *node)
{
    _uncompilationTable.AddRulesForNode(node->GetId(), journal);
}

void
Exec_Program::_RegisterInputNode(const Exec_AttributeInputNode *const inputNode)
{
    const auto [it, emplaced] = _inputNodes.emplace(
        inputNode->GetAttributePath(), inputNode->GetId());
    TF_VERIFY(emplaced);
}

void
Exec_Program::_UnregisterInputNode(
    const Exec_AttributeInputNode *const inputNode)
{
    const SdfPath attributePath = inputNode->GetAttributePath();
    const size_t numErased = _inputNodes.unsafe_erase(attributePath);
    TF_VERIFY(numErased);
}

PXR_NAMESPACE_CLOSE_SCOPE
