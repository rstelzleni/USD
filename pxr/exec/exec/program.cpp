//
// Copyright 2025 Pixar
//
// Licensed under the terms set forth in the LICENSE.txt file available at
// https://openusd.org/license.
//
#include "pxr/exec/exec/program.h"

#include "pxr/base/tf/span.h"
#include "pxr/base/tf/token.h"
#include "pxr/exec/ef/timeInputNode.h"
#include "pxr/exec/ef/timeInterval.h"
#include "pxr/exec/vdf/grapher.h"
#include "pxr/exec/vdf/maskedOutput.h"
#include "pxr/exec/vdf/node.h"
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

void
Exec_Program::InvalidateAuthoredValues(
    TfSpan<ExecInvalidAuthoredValue> invalidProperties)
{
    // TODO: Leaf node and request invalidation, as well as queueing up main
    // executor invalidation. 
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

PXR_NAMESPACE_CLOSE_SCOPE
