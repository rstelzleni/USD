//
// Copyright 2025 Pixar
//
// Licensed under the terms set forth in the LICENSE.txt file available at
// https://openusd.org/license.
//
#include "pxr/exec/exec/uncompiler.h"

#include "pxr/exec/exec/program.h"
#include "pxr/exec/exec/runtime.h"
#include "pxr/exec/exec/uncompilationRuleSet.h"
#include "pxr/exec/exec/uncompilationTable.h"

#include "pxr/base/tf/token.h"
#include "pxr/base/trace/trace.h"
#include "pxr/exec/esf/editReason.h"
#include "pxr/usd/sdf/path.h"

#include <vector>

PXR_NAMESPACE_OPEN_SCOPE

void
Exec_Uncompiler::UncompileForSceneChange(
    const SdfPath &path,
    const EsfEditReason editReasons)
{
    if (editReasons == EsfEditReason::None) {
        return;
    }

    TRACE_FUNCTION();

    if (editReasons & EsfEditReason::ResyncedObject) {
        // Resyncs are recursive, so we need to process resyncs for the changed
        // path, and for all descendant paths. This simultaneously removes the
        // matching rule sets from the uncompilation table.
        const std::vector<Exec_UncompilationTable::Entry> tableEntries =
            _program->ExtractUncompilationRuleSetsForResync(path);

        for (const Exec_UncompilationTable::Entry &tableEntry : tableEntries) {
            _ProcessUncompilationRuleSet(
                tableEntry.path, 
                editReasons, 
                tableEntry.ruleSet.get());
        }
        return;
    }

    // For non-resync changes, we only process a single rule set for the changed
    // path.
    const Exec_UncompilationTable::Entry tableEntry =
        _program->GetUncompilationRuleSetForPath(path);
    
    // If there are no rules for this path, then there's nothing to do.
    if (!tableEntry.ruleSet) {
        return;
    }

    _ProcessUncompilationRuleSet(
        tableEntry.path,
        editReasons, 
        tableEntry.ruleSet.get());
}

void
Exec_Uncompiler::_ProcessUncompilationRuleSet(
    const SdfPath &path,
    const EsfEditReason editReasons,
    Exec_UncompilationRuleSet *const ruleSet)
{
    TRACE_FUNCTION();

    // TODO: Add debug flags to trace the uncompilation process, in which
    // logging the path will be helpful.
    (void)path;

    Exec_UncompilationRuleSet::iterator ruleSetIter = ruleSet->begin();
    while (ruleSetIter != ruleSet->end()) {
        const Exec_UncompilationRule &rule = *ruleSetIter;
        
        // If the rule pertains to a node that no longer exists, then we
        // "garbage collect" that rule from the rule set. This can happen if
        // uncompilation rules for another path uncompiled the same object in
        // the network.
        VdfNode *const node = _program->GetNodeById(rule.nodeId);
        if (!node) {
            if (editReasons & EsfEditReason::ResyncedObject) {
                // If the change is a recursive resync, don't bother erasing the
                // individual rule, because the entire rule set is already going
                // to be destroyed.
                ++ruleSetIter;
            } else {
                ruleSetIter = ruleSet->erase(ruleSetIter);
            }
            continue;
        }

        // Skip this rule if its edit reasons are not applicable to this change.
        if (!(rule.reasons & editReasons)) {
            ++ruleSetIter;
            continue;
        }

        // If the rule's input name is empty, then the entire node should be
        // uncompiled. Otherwise, only uncompile the input on that node.
        if (rule.inputName.IsEmpty()) {
            _program->DisconnectAndDeleteNode(node);
            _runtime->ClearData(*node);
            _didUncompile = true;
        }
        else {
            // TODO: Disconnecting the input does not delete the node, nor does
            // it delete the input. This means that other rules targeting this
            // input remain active, even though the input was uncompiled. To
            // handle this, we need to implement a tombstone mechanism to
            // deactivate those rules, which can be added in a future version.
            // For now, the only supported edit reason is Resync which prevents
            // this from being a problem, but it needs to be corrected when
            // we handle namespace edits.
            _program->DisconnectInput(node->GetInput(rule.inputName));
            _didUncompile = true;
        }

        // The rule has triggered and is no longer valid.
        ruleSetIter = ruleSet->erase(ruleSetIter);
    }
}

PXR_NAMESPACE_CLOSE_SCOPE
