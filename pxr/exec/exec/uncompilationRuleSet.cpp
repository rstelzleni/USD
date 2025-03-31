//
// Copyright 2025 Pixar
//
// Licensed under the terms set forth in the LICENSE.txt file available at
// https://openusd.org/license.
//
#include "pxr/exec/exec/uncompilationRuleSet.h"

#include "pxr/base/tf/diagnosticLite.h"
#include "pxr/exec/vdf/node.h"

#include <iterator>
#include <sstream>

PXR_NAMESPACE_OPEN_SCOPE

Exec_UncompilationRule::Exec_UncompilationRule()
    : nodeId(0)
{
    TF_VERIFY(false, "Exec_UncompilationRule has no reasonable default");
}

Exec_UncompilationRuleSet::iterator
Exec_UncompilationRuleSet::erase(
    const Exec_UncompilationRuleSet::iterator &it)
{
    iterator last = std::prev(_rules.end());
    if (it != last) {
        std::swap(*it, *last);
    }
    _rules.resize(_rules.size() - 1);
    return it;
}

std::string
Exec_UncompilationRuleSet::GetDescription() const
{
    if (_rules.empty()) {
        return "{}";
    }
    
    std::ostringstream str;
    str << "{\n";
    for (const Exec_UncompilationRule &rule : _rules) {
        str << "("
            << VdfNode::GetVersionFromId(rule.nodeId) << ", "
            << VdfNode::GetIndexFromId(rule.nodeId);
        if (!rule.inputName.IsEmpty()) {
            str << ", " << rule.inputName.GetText();
        }
        str << "): "
            << "[" << rule.reasons.GetDescription() << "],\n";
    }
    str << '}';

    return str.str();
}

PXR_NAMESPACE_CLOSE_SCOPE
