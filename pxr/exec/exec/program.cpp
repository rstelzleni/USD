//
// Copyright 2025 Pixar
//
// Licensed under the terms set forth in the LICENSE.txt file available at
// https://openusd.org/license.
//
#include "pxr/exec/exec/program.h"

#include "pxr/exec/exec/authoredValueInvalidationResult.h"
#include "pxr/exec/exec/parallelForRange.h"
#include "pxr/exec/exec/timeChangeInvalidationResult.h"

#include "pxr/base/tf/span.h"
#include "pxr/base/tf/token.h"
#include "pxr/base/trace/trace.h"
#include "pxr/base/work/loops.h"
#include "pxr/base/work/withScopedParallelism.h"
#include "pxr/exec/ef/time.h"
#include "pxr/exec/ef/timeInputNode.h"
#include "pxr/exec/ef/timeInterval.h"
#include "pxr/exec/vdf/connection.h"
#include "pxr/exec/vdf/executorInterface.h"
#include "pxr/exec/vdf/grapher.h"
#include "pxr/exec/vdf/input.h"
#include "pxr/exec/vdf/maskedOutput.h"
#include "pxr/exec/vdf/maskedOutputVector.h"
#include "pxr/exec/vdf/node.h"
#include "pxr/exec/vdf/typedVector.h"
#include "pxr/exec/vdf/types.h"
#include "pxr/usd/sdf/path.h"

#include <tbb/enumerable_thread_specific.h>

PXR_NAMESPACE_OPEN_SCOPE

static TfBits
_FilterTimeDependentInputs(
    const VdfMaskedOutputVector &, const EfTime &, const EfTime &);

class Exec_Program::_EditMonitor final : public VdfNetwork::EditMonitor {
public:
    explicit _EditMonitor(Exec_Program *const program)
        : _program(program)
    {}

    void WillClear() override {
        _program->_leafNodeCache.Clear();
    }

    void DidConnect(const VdfConnection *connection) override {
        _program->_leafNodeCache.DidConnect(*connection);
    }

    void DidAddNode(const VdfNode *node) override {}

    void WillDelete(const VdfConnection *connection) override {
        _program->_leafNodeCache.WillDeleteConnection(*connection);
    }

    void WillDelete(const VdfNode *node) override {
        // TODO: When we implement parallel node deletion, this needs to be made
        // thread-safe.
        _program->_compiledOutputCache.EraseByNodeId(node->GetId());
    }

private:
    Exec_Program *const _program;
};

Exec_Program::Exec_Program()
    : _timeInputNode(new EfTimeInputNode(&_network))
    , _timeDependentInputsValid(true)
    , _editMonitor(std::make_unique<_EditMonitor>(this))
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

Exec_AuthoredValueInvalidationResult
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
    bool isTimeDependencyChange = false;

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
        _InputNodeEntry &entry = it->second;
        VdfNode *const node = _network.GetNodeById(entry.nodeId);

        // We expect uncompiled input nodes to have been removed from the
        // _inputNodes array by now.
        if (!TF_VERIFY(node)) {
            continue;
        }

        // Figure out if the input node's time dependence has changed based on
        // the authored value change.
        const Exec_AttributeInputNode *const inputNode =
            dynamic_cast<Exec_AttributeInputNode*>(node);
        if (TF_VERIFY(inputNode)) {
            const bool isTimeDependent = inputNode->MaybeTimeVarying();
            if (entry.isTimeDependent != isTimeDependent) {
                _InvalidateTimeDependentInputs();
                isTimeDependencyChange = true;
                entry.isTimeDependent = isTimeDependent;
            }
        }

        // If this is an input node to the exec network, we need to make sure
        // that it is re-initialized before the next round of evaluation.
        _uninitializedInputNodes.push_back(entry.nodeId);

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

    return Exec_AuthoredValueInvalidationResult{
        invalidProperties,
        std::move(compiledProperties),
        leafNodes,
        totalInvalidInterval,
        isTimeDependencyChange};
}

Exec_TimeChangeInvalidationResult
Exec_Program::InitializeTime(
    const EfTime &newTime,
    VdfExecutorInterface *const executor)
{
    // Returned as a sentinel if there are no invalid leaf nodes.
    static const std::vector<const VdfNode *> noInvalidLeafNodes;

    // Retrieve the old time from the executor data manager.
    const VdfOutput &timeOutput = *_timeInputNode->GetOutput();
    const VdfVector *const oldTimeVector = executor->GetOutputValue(
        timeOutput, VdfMask::AllOnes(1));

    // If there isn't already a time value stored in the executor data manager,
    // perform first time initialization and return.
    if (!oldTimeVector) {
        executor->SetOutputValue(
            timeOutput, VdfTypedVector<EfTime>(newTime), VdfMask::AllOnes(1));
        return Exec_TimeChangeInvalidationResult{
            noInvalidLeafNodes, newTime, newTime};
    }

    // Get the old time value from the vector. If there is no change in time,
    // we can return without performing invalidation.
    const EfTime oldTime = oldTimeVector->GetReadAccessor<EfTime>()[0];
    if (oldTime == newTime) {
        return Exec_TimeChangeInvalidationResult{
            noInvalidLeafNodes, oldTime, newTime};
    }

    TRACE_FUNCTION();

    // Gather up the set of inputs that are currently time-dependent.
    const VdfMaskedOutputVector &timeDependentInputs =
        _CollectTimeDependentInputs();

    // When moving to- or from the default time, we need to invalidate all
    // time-dependent inputs.
    const bool didChangeDefault = 
        oldTime.GetTimeCode().IsDefault() != newTime.GetTimeCode().IsDefault();
    
    // Construct a bit set that filters the array of time dependent inputs down
    // to the ones that actually changed going from oldTime to newTime.
    const TfBits filter = didChangeDefault
        ? TfBits(timeDependentInputs.size(), 0, timeDependentInputs.size() - 1)
        : _FilterTimeDependentInputs(timeDependentInputs, oldTime, newTime);

    // Perform executor invalidation, and notify requests of the time change.
    const std::vector<const VdfNode *> *leafNodes = nullptr;
    WorkWithScopedDispatcher(
        [&filter, &timeDependentInputs, &executor, &timeOutput, &newTime,
            &leafNodes, &leafNodeCache = _leafNodeCache]
        (WorkDispatcher &dispatcher){
        // Executor invalidation task.
        dispatcher.Run([&](){
            // Turn the invalid time-dependent inputs into a request.
            VdfMaskedOutputVector invalidationRequest;
            invalidationRequest.reserve(filter.GetNumSet());
            for (size_t i : filter.GetAllSetView()) {
                invalidationRequest.push_back(timeDependentInputs[i]);
            }

            // Invalidate values on the executor.
            executor->InvalidateValues(invalidationRequest);

            // Set the new time value on the executor.
            executor->SetOutputValue(
                timeOutput,
                VdfTypedVector<EfTime>(newTime),
                VdfMask::AllOnes(1));
        });

        // Find invalid leaf nodes and notify requests.
        dispatcher.Run([&](){
            // Find the leaf nodes that are dependent on the values that are
            // changing from oldTime to newTime.
            leafNodes = &(leafNodeCache.FindNodes(timeDependentInputs, filter));
        });
    });

    TF_VERIFY(leafNodes);
    return Exec_TimeChangeInvalidationResult{*leafNodes, oldTime, newTime};
}

void
Exec_Program::InitializeInputNodes(VdfExecutorInterface *const executor)
{
    if (_uninitializedInputNodes.empty()) {
        return;
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

    // Make sure that the executor data manager is properly invalidated for any
    // input nodes that were just initialized.
    if (!invalidationRequest.empty()) {
        executor->InvalidateValues(invalidationRequest);
    }
}

void
Exec_Program::DisconnectAndDeleteNode(VdfNode *const node)
{
    TRACE_FUNCTION();

    // Track a set of connections to be deleted at the end of this function,
    // because it is not safe to remove connections while iterating over them.
    VdfConnectionVector connections;

    // Upstream nodes are potentially isolated.
    for (const auto &[name, input] : node->GetInputsIterator()) {
        TF_UNUSED(name);
        for (VdfConnection *const connection : input->GetConnections()) {
            _potentiallyIsolatedNodes.insert(&connection->GetSourceNode());
            connections.push_back(connection);
        }
    }

    // Downstream inputs require recompilation.
    for (const auto &[name, output] : node->GetOutputsIterator()) {
        TF_UNUSED(name);
        for (VdfConnection *const connection : output->GetConnections()) {
            _inputsRequiringRecompilation.insert(&connection->GetTargetInput());

            // TODO: We currently disconnect other connections incoming on the
            // target input, and we mark the nodes upstream of those connections
            // as potentially isolated. We do this because recompilation of
            // inputs expects those inputs to be fully disconnected. However,
            // a future change can add support to recompile inputs with existing
            // connections.
            for (VdfConnection *const targetInputConnection :
                connection->GetTargetInput().GetConnections()) {
                
                _potentiallyIsolatedNodes.insert(
                    &targetInputConnection->GetSourceNode());
                connections.push_back(targetInputConnection);
            }
        }
    }

    // Unregister this node if it is an attribute input node.
    if (const Exec_AttributeInputNode *const inputNode = 
            dynamic_cast<const Exec_AttributeInputNode *const>(node)) {
        _UnregisterInputNode(inputNode);
    }

    // This node cannot be isolated, and its inputs do not require
    // recompilation, because they are all about to be deleted.
    _potentiallyIsolatedNodes.erase(node);
    for (const auto &[name, input] : node->GetInputsIterator()) {
        TF_UNUSED(name);
        _inputsRequiringRecompilation.erase(input);
    }

    // Finally, delete the affected connections and the node.
    for (VdfConnection *const connection : connections) {
        _network.Disconnect(connection);
    }
    _network.Delete(node);
}

void
Exec_Program::DisconnectInput(VdfInput *const input)
{
    TRACE_FUNCTION();

    _inputsRequiringRecompilation.insert(input);

    // All source nodes of the input's connections are now potentially isolated.
    // Iterate over a copy of the connections, because the original vector will
    // be modified by VdfNetwork::Disconnect.
    const VdfConnectionVector connections = input->GetConnections();
    for (VdfConnection *const connection : connections) {
        _potentiallyIsolatedNodes.insert(&connection->GetSourceNode());
        _network.Disconnect(connection);
    }
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
    const bool isTimeDependent = inputNode->MaybeTimeVarying();
    const auto [it, emplaced] = _inputNodes.emplace(
        inputNode->GetAttributePath(), 
        _InputNodeEntry{inputNode->GetId(), isTimeDependent});
    TF_VERIFY(emplaced);

    // If this is a time varying input, we need to invalidate the cached
    // subset of time varying input nodes.
    if (isTimeDependent) {
        _InvalidateTimeDependentInputs();
    }
}

void
Exec_Program::_UnregisterInputNode(
    const Exec_AttributeInputNode *const inputNode)
{
    const SdfPath attributePath = inputNode->GetAttributePath();
    const auto it = _inputNodes.find(attributePath);
    if (!TF_VERIFY(it != _inputNodes.end())) {
        return;
    }

    // If this was a time varying input, we need to invalidate the cached
    // subset of time varying input nodes.
    const bool wasTimeDependent = it->second.isTimeDependent;
    if (wasTimeDependent) {
        _InvalidateTimeDependentInputs();
    }
    
    _inputNodes.unsafe_erase(attributePath);
}

void
Exec_Program::_InvalidateTimeDependentInputs()
{
    // We set an atomic flag here instead of fiddling with the
    // _timeDependentInputs array directly, so that we don't have to worry
    // about making the latter a concurrent data structure.
    _timeDependentInputsValid.store(false, std::memory_order_release);
}

const VdfMaskedOutputVector &
Exec_Program::_CollectTimeDependentInputs()
{
    // If the cached array of time-dependent inputs is still valid, return it.
    if (_timeDependentInputsValid.load(std::memory_order_acquire)) {
        return _timeDependentInputs;
    }

    TRACE_FUNCTION();

    // To allow us to rebuild the array of time-dependent inputs in parallel,
    // pessimally size it to accommodate all inputs, and keep track of how many
    // entries have really been populated. We will shrink the array later.
    std::atomic<size_t> num(0);
    _timeDependentInputs.resize(_inputNodes.size());

    // Iterate over all the inputs and filter the ones that are currently time
    // dependent.
    Exec_ParallelForRange(_inputNodes,
        [&num, &result = _timeDependentInputs, &network = _network]
        (const _InputNodesMap::range_type &range){
        for (auto it = range.begin(); it != range.end(); ++it) {
            const _InputNodeEntry &entry = it->second;
            if (entry.isTimeDependent) {
                VdfNode *const node = network.GetNodeById(entry.nodeId);
                if (TF_VERIFY(node)) {
                    result[num++] = VdfMaskedOutput(
                        node->GetOutput(), VdfMask::AllOnes(1));
                }
            }
        }
    });

    // Shrink the array to only contain the entries we just populated.
    _timeDependentInputs.resize(num.load());

    // The array of time-dependent inputs is valid again. Return it.
    _timeDependentInputsValid.store(true, std::memory_order_release);
    return _timeDependentInputs;
}

static TfBits
_FilterTimeDependentInputs(
    const VdfMaskedOutputVector &timeDependentInputs,
    const EfTime &oldTime,
    const EfTime &newTime)
{
    TRACE_FUNCTION();

    // One bitset per thread.
    const size_t numInputs = timeDependentInputs.size();
    tbb::enumerable_thread_specific<TfBits> threadBits([numInputs](){
        return TfBits(numInputs);
    });

    // For each time-dependent input, figure out if the input value actually
    // changes between oldTime and newTime. If so, set the corresponding bit
    // in the bit set.
    WorkWithScopedParallelism(
        [&numInputs, &timeDependentInputs, &oldTime, &newTime, &threadBits](){
        WorkParallelForN(numInputs,[&](size_t b, size_t e){
            TfBits *const bits = &threadBits.local();
            for (size_t i = b; i != e; ++i) {
                const VdfNode &node =
                    timeDependentInputs[i].GetOutput()->GetNode();
                const Exec_AttributeInputNode *const inputNode =
                        dynamic_cast<const Exec_AttributeInputNode*>(&node);
                if (TF_VERIFY(inputNode)) {
                    if (inputNode->IsTimeVarying(oldTime, newTime)) {
                        bits->Set(i);
                    }
                }
            }
        });
    });

    // Combine the thread-local bit sets into a single bit set and return it.
    return threadBits.combine([](const TfBits &lhs, const TfBits &rhs){
        return lhs | rhs;
    });
}

PXR_NAMESPACE_CLOSE_SCOPE
