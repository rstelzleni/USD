//
// Copyright 2025 Pixar
//
// Licensed under the terms set forth in the LICENSE.txt file available at
// https://openusd.org/license.
//
#include "pxr/exec/exec/cacheView.h"

#include "pxr/exec/exec/valueExtractor.h"

#include "pxr/exec/vdf/maskedOutput.h"
#include "pxr/exec/vdf/vector.h"

#include "pxr/base/vt/value.h"

PXR_NAMESPACE_OPEN_SCOPE

Exec_CacheView::Exec_CacheView(
    const VdfDataManagerFacade dataManager,
    TfSpan<const VdfMaskedOutput> outputs,
    TfSpan<const Exec_ValueExtractor> extractors)
    : _dataManager(
        // Set the view to an invalid state if the outputs and extractors
        // don't line up.
        TF_VERIFY(outputs.size() == extractors.size())
        ? std::optional(dataManager)
        : std::nullopt)
    , _outputs(outputs)
    , _extractors(extractors)
{
}

bool
Exec_CacheView::Extract(int idx, VtValue *result) const
{
    if (!_dataManager) {
        TF_CODING_ERROR("Cannot extract from invalid view");
        return false;
    }

    if (!result) {
        TF_CODING_ERROR("Got NULL result");
        return false;
    }

    if (!(0 <= idx && idx < static_cast<int>(_outputs.size()))) {
        TF_CODING_ERROR("Index '%d' out of range", idx);
        return false;
    }

    const VdfMaskedOutput &mo = _outputs[idx];
    if (!TF_VERIFY(mo)) {
        return false;
    }

    const VdfVector *v = _dataManager->GetOutputValue(
        *mo.GetOutput(), mo.GetMask());
    if (!v) {
        TF_CODING_ERROR("No value cached for output '%s' (idx=%d)",
                        mo.GetDebugName().c_str(), idx);
        return false;
    }

    const Exec_ValueExtractor extractor = _extractors[idx];
    *result = extractor(*v, mo.GetMask());
    return true;
}

PXR_NAMESPACE_CLOSE_SCOPE
