//
// Copyright 2025 Pixar
//
// Licensed under the terms set forth in the LICENSE.txt file available at
// https://openusd.org/license.
//
#ifndef PXR_EXEC_EXEC_PROGRAM_H
#define PXR_EXEC_EXEC_PROGRAM_H

#include "pxr/pxr.h"

#include "pxr/exec/exec/attributeInputNode.h"
#include "pxr/exec/exec/compiledOutputCache.h"
#include "pxr/exec/exec/types.h"
#include "pxr/exec/exec/uncompilationTable.h"

#include "pxr/base/tf/bits.h"
#include "pxr/exec/ef/leafNodeCache.h"
#include "pxr/exec/ef/time.h"
#include "pxr/exec/vdf/maskedOutput.h"
#include "pxr/exec/vdf/maskedOutputVector.h"
#include "pxr/exec/vdf/network.h"
#include "pxr/exec/vdf/types.h"
#include "pxr/usd/sdf/path.h"

#include <tbb/concurrent_unordered_map.h>

#include <atomic>
#include <memory>
#include <tuple>
#include <type_traits>
#include <unordered_set>
#include <vector>

PXR_NAMESPACE_OPEN_SCOPE

class EfTime;
class EfTimeInputNode;
class EsfJournal;
class Exec_AuthoredValueInvalidationResult;
class Exec_TimeChangeInvalidationResult;
class TfBits;
template <typename> class TfSpan;
class VdfExecutorInterface;
class VdfInput;
class VdfNode;

/// Owns a VdfNetwork and related data structures to access and modify the
/// network.
///
/// The VdfNetwork describes the topological structure of nodes and connections,
/// but does not prescribe any meaning to the organization of the network. In
/// order to compile, update, and evaluate the network, Exec requires additional
/// metadata to facilitate common access patterns.
///
/// Generally, the data structures contained by this class are those that must
/// have exactly one instance per-network. The responsibilities of these data
/// structures include:
///
///   - Tracking which VdfOutput provides the value of a given Exec_OutputKey.
///   - Tracking the conditions when specific nodes and connections should be
///     deleted from the network.
///   - Tracking the leaf nodes dependent on any particular output in the
///     network.
///   - Tracking which nodes may be isolated due to uncompilation.
///   - Tracking which inputs have been affected by uncompilation and should
///     later be recompiled.
///
/// Some of these data structures must be modified when the network is modified.
/// Therefore, compilation never directly accesses the VdfNetwork, but does so
/// through an Exec_Program.
///
class Exec_Program
{
public:
    Exec_Program();

    // Non-copyable and non-movable.
    Exec_Program(const Exec_Program &) = delete;
    Exec_Program& operator=(const Exec_Program &) = delete;
    
    ~Exec_Program();

    /// Adds a new node in the VdfNetwork.
    ///
    /// Constructs a node of type \p NodeType. The first argument of the node
    /// constructor is a pointer to the VdfNetwork maintained by this
    /// Exec_Program, and the remaining arguments are forwarded from
    /// \p nodeCtorArgs.
    ///
    /// Uncompilation rules for the new node are added from the \p journal.
    ///
    /// \return a pointer to the newly constructed node. This pointer is owned
    /// by the network.
    ///
    template <class NodeType, class... NodeCtorArgs>
    NodeType *CreateNode(
        const EsfJournal &journal,
        NodeCtorArgs... nodeCtorArgs);

    /// Makes connections between nodes in the VdfNetwork.
    ///
    /// All non-null VdfMaskedOutputs in \p outputs are connected to the input
    /// named \p inputName on \p inputNode. Null outputs are skipped.
    ///
    /// Even if \p outputs is empty or lacks non-null outputs, this method
    /// should still be called in order to properly add uncompilation rules from
    /// the \p journal.
    ///
    void Connect(
        const EsfJournal &journal,
        TfSpan<const VdfMaskedOutput> outputs,
        VdfNode *inputNode,
        const TfToken &inputName);

    /// Gets the VdfMaskedOutput provided by \p outputKeyIdentity.
    ///
    /// \return a pair containing the matching VdfMaskedOutput and a bool
    /// indicating whether there exists an output for the given
    /// \p outputKeyIdentity.
    ///
    /// \note
    /// If the returned boolean is true, the returned VdfMaskedOutput may still
    /// contain a null VdfOutput. This indicates that the given output key is
    /// *already known* to not have a corresponding output.
    ///
    std::tuple<const VdfMaskedOutput &, bool> GetCompiledOutput(
        const Exec_OutputKey::Identity &outputKeyIdentity) const {
        return _compiledOutputCache.Find(outputKeyIdentity);
    }

    /// Establishes that \p outputKeyIdentity is provided by \p maskedOutput.
    ///
    /// If \p outputKeyIdentity has not yet been mapped to a masked output,
    /// insert the new mapping and return true. Otherwise, the existing mapping
    /// is not modified, and this returns false.
    ///
    bool SetCompiledOutput(
        const Exec_OutputKey::Identity &outputKeyIdentity,
        const VdfMaskedOutput &maskedOutput) {
        return _compiledOutputCache.Insert(outputKeyIdentity, maskedOutput);
    }

    /// Returns the current generational counter of the execution network.
    size_t GetNetworkVersion() const {
        return _network.GetVersion();
    }

    /// Notifies the program of authored value invalidation.
    ///
    /// \return a vector of invalid leaf nodes, a bit set indicating which 
    /// \p invalidProperties are compiled, and a time interval indicating the
    /// combined invalidation interval.
    /// 
    Exec_AuthoredValueInvalidationResult InvalidateAuthoredValues(
        TfSpan<ExecInvalidAuthoredValue> invalidProperties);

    /// Initializes all invalid input nodes in the network.
    void InitializeInputNodes(VdfExecutorInterface *executor);

    /// Initializes time in the network.
    Exec_TimeChangeInvalidationResult InitializeTime(
        const EfTime &newTime, VdfExecutorInterface *executor);

    /// Returns the node with the given \p nodeId, or nullptr if no such node
    /// exists.
    ///
    VdfNode *GetNodeById(const VdfId nodeId) {
        return _network.GetNodeById(nodeId);
    }

    /// Deletes a \p node from the network.
    ///
    /// All incoming and outgoing connections on \p node are deleted. Downstream
    /// inputs previously connected to \p node are marked as "dirty" and can be
    /// queried by GetDirtyOutputs. Upstream nodes previously feeding into
    /// \p node may be left isolated.
    ///
    /// \note
    /// This method is not thread-safe.
    ///
    void DisconnectAndDeleteNode(VdfNode *node);

    /// Deletes all connections flowing into \p input.
    ///
    /// This input is added to the set of "dirty" inputs. Upstream nodes
    /// previously feeding into this \p input may be left isolated.
    ///
    /// \note
    /// This method is not thread-safe.
    /// 
    void DisconnectInput(VdfInput *input);

    /// Gets the set of inputs that have been affected by uncompilation and need
    /// to be recompiled.
    ///
    const std::unordered_set<VdfInput *> &
    GetInputsRequiringRecompilation() const {
        return _inputsRequiringRecompilation;
    }

    /// Clears the set of inputs that were affected by uncompilation.
    ///
    /// This should be called after all such inputs have been recompiled.
    ///
    /// \note
    /// This method is not thread-safe.
    ///
    void ClearInputsRequiringRecompilation() {
        _inputsRequiringRecompilation.clear();
    }

    /// Returns Exec_UncompilationRuleSet%s for \p resyncedPath and descendants
    /// of \p resyncedPath.
    ///
    std::vector<Exec_UncompilationTable::Entry>
    ExtractUncompilationRuleSetsForResync(const SdfPath &resyncedPath) {
        return _uncompilationTable.UpdateForRecursiveResync(resyncedPath);
    }

    /// Returns the Exec_UncompilationRuleSet for \p changedPath.
    Exec_UncompilationTable::Entry GetUncompilationRuleSetForPath(
        const SdfPath &changedPath) {
        return _uncompilationTable.Find(changedPath);
    }

    /// Writes the compiled network to a file at \p filename.
    void GraphNetwork(const char *filename) const;

private:
    // Updates data structures for a newly-added node.
    void _AddNode(const EsfJournal &journal, const VdfNode *node);

    // Registers an input node for authored value initialization.
    void _RegisterInputNode(const Exec_AttributeInputNode *inputNode);

    // Unregisters an input node from authored value initialization.
    void _UnregisterInputNode(const Exec_AttributeInputNode *inputNode);

    // Flags the array of _timeDependentInputs as invalid.
    void _InvalidateTimeDependentInputs();

    // Rebuilds the array of _timeDependentInputs.
    const VdfMaskedOutputVector &_CollectTimeDependentInputs();

private:
    // The compiled data flow network.
    VdfNetwork _network;

    // Every network always has a compiled time input node.
    EfTimeInputNode *const _timeInputNode;

    // A cache of compiled outputs keys and corresponding data flow outputs.
    Exec_CompiledOutputCache _compiledOutputCache;

    // Maps scene paths to data flow network that must be uncompiled in response
    // to edits to those scene paths.
    Exec_UncompilationTable _uncompilationTable;

    // Collection of compiled leaf nodes.
    EfLeafNodeCache _leafNodeCache;

    // Collection of compiled input nodes.
    struct _InputNodeEntry {
        VdfId nodeId;
        bool isTimeDependent;
    };
    using _InputNodesMap =
        tbb::concurrent_unordered_map<SdfPath, _InputNodeEntry, SdfPath::Hash>;
    _InputNodesMap _inputNodes;

    // Array of outputs on input nodes, which are time dependent.
    VdfMaskedOutputVector _timeDependentInputs;

    // Flag indicating whether the _timeDependentInputs array is up-to-date or
    // must be re-computed.
    std::atomic<bool> _timeDependentInputsValid;

    // Input nodes currently queued for initialization.
    std::vector<VdfId> _uninitializedInputNodes;

    // On behalf of the program intercepts and responds to fine-grained network
    // edits.
    class _EditMonitor;
    std::unique_ptr<_EditMonitor> _editMonitor;

    // Nodes that may be isolated due to prior uncompilation.
    std::unordered_set<VdfNode *> _potentiallyIsolatedNodes;

    // Inputs that were disconnected during uncompilation.
    std::unordered_set<VdfInput *> _inputsRequiringRecompilation;
};


template <class NodeType, class... NodeCtorArgs>
NodeType *Exec_Program::CreateNode(
    const EsfJournal &journal,
    NodeCtorArgs... nodeCtorArgs)
{
    static_assert(std::is_base_of_v<VdfNode, NodeType>);

    // The time node is a special case.
    if constexpr (std::is_same_v<EfTimeInputNode, NodeType>) {
        return _timeInputNode;
    }

    NodeType *const node = new NodeType(
        &_network, std::forward<NodeCtorArgs>(nodeCtorArgs)...);
    _AddNode(journal, node);

    // Input nodes are tracked for authored value initialization.
    if constexpr (std::is_same_v<Exec_AttributeInputNode, NodeType>) {
        _RegisterInputNode(node);
    }

    return node;
}

PXR_NAMESPACE_CLOSE_SCOPE

#endif
