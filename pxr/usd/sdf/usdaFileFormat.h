//
// Copyright 2016 Pixar
//
// Licensed under the terms set forth in the LICENSE.txt file available at
// https://openusd.org/license.
//
#ifndef PXR_USD_SDF_USDA_FILE_FORMAT_H
#define PXR_USD_SDF_USDA_FILE_FORMAT_H
 
#include "pxr/pxr.h"
#include "pxr/usd/usd/api.h"
#include "pxr/usd/sdf/textFileFormat.h"
#include "pxr/base/tf/declarePtrs.h"
#include "pxr/base/tf/staticTokens.h"

PXR_NAMESPACE_OPEN_SCOPE

#define SDF_USDA_FILE_FORMAT_TOKENS \
    ((Id,      "usda"))             \
    ((Version, "1.0"))

TF_DECLARE_PUBLIC_TOKENS(SdfUsdaFileFormatTokens, SDF_API, SDF_USDA_FILE_FORMAT_TOKENS);

TF_DECLARE_WEAK_AND_REF_PTRS(SdfUsdaFileFormat);

/// \class SdfUsdaFileFormat
///
/// File format used by textual USD files.
///
class SdfUsdaFileFormat : public SdfTextFileFormat
{
private:
    SDF_FILE_FORMAT_FACTORY_ACCESS;

    SdfUsdaFileFormat();
    virtual ~SdfUsdaFileFormat();

    friend class SdfUsdFileFormat;
};

PXR_NAMESPACE_CLOSE_SCOPE

#endif // PXR_USD_SDF_USDA_FILE_FORMAT_H
