//
// Copyright 2023 Pixar
//
// Licensed under the Apache License, Version 2.0 (the "Apache License")
// with the following modification; you may not use this file except in
// compliance with the Apache License and the following modification to it:
// Section 6. Trademarks. is deleted and replaced with:
//
// 6. Trademarks. This License does not grant permission to use the trade
//    names, trademarks, service marks, or product names of the Licensor
//    and its affiliates, except as required to comply with Section 4(c) of
//    the License and to reproduce the content of the NOTICE file.
//
// You may obtain a copy of the Apache License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the Apache License with the above modification is
// distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
// KIND, either express or implied. See the Apache License for the specific
// language governing permissions and limitations under the Apache License.
//
////////////////////////////////////////////////////////////////////////

/* ************************************************************************** */
/* **                                                                      ** */
/* ** This file is generated by a script.                                  ** */
/* **                                                                      ** */
/* ** Do not edit it directly (unless it is within a CUSTOM CODE section)! ** */
/* ** Edit hdSchemaDefs.py instead to make changes.                        ** */
/* **                                                                      ** */
/* ************************************************************************** */

#include "pxr/imaging/hd/dependencySchema.h"

#include "pxr/imaging/hd/retainedDataSource.h"

#include "pxr/base/trace/trace.h"

// --(BEGIN CUSTOM CODE: Includes)--
// --(END CUSTOM CODE: Includes)--

PXR_NAMESPACE_OPEN_SCOPE

TF_DEFINE_PUBLIC_TOKENS(HdDependencySchemaTokens,
    HD_DEPENDENCY_SCHEMA_TOKENS);

// --(BEGIN CUSTOM CODE: Schema Methods)--
// --(END CUSTOM CODE: Schema Methods)--

HdPathDataSourceHandle
HdDependencySchema::GetDependedOnPrimPath() const
{
    return _GetTypedDataSource<HdPathDataSource>(
        HdDependencySchemaTokens->dependedOnPrimPath);
}

HdLocatorDataSourceHandle
HdDependencySchema::GetDependedOnDataSourceLocator() const
{
    return _GetTypedDataSource<HdLocatorDataSource>(
        HdDependencySchemaTokens->dependedOnDataSourceLocator);
}

HdLocatorDataSourceHandle
HdDependencySchema::GetAffectedDataSourceLocator() const
{
    return _GetTypedDataSource<HdLocatorDataSource>(
        HdDependencySchemaTokens->affectedDataSourceLocator);
}

/*static*/
HdContainerDataSourceHandle
HdDependencySchema::BuildRetained(
        const HdPathDataSourceHandle &dependedOnPrimPath,
        const HdLocatorDataSourceHandle &dependedOnDataSourceLocator,
        const HdLocatorDataSourceHandle &affectedDataSourceLocator
)
{
    TfToken _names[3];
    HdDataSourceBaseHandle _values[3];

    size_t _count = 0;

    if (dependedOnPrimPath) {
        _names[_count] = HdDependencySchemaTokens->dependedOnPrimPath;
        _values[_count++] = dependedOnPrimPath;
    }

    if (dependedOnDataSourceLocator) {
        _names[_count] = HdDependencySchemaTokens->dependedOnDataSourceLocator;
        _values[_count++] = dependedOnDataSourceLocator;
    }

    if (affectedDataSourceLocator) {
        _names[_count] = HdDependencySchemaTokens->affectedDataSourceLocator;
        _values[_count++] = affectedDataSourceLocator;
    }
    return HdRetainedContainerDataSource::New(_count, _names, _values);
}

HdDependencySchema::Builder &
HdDependencySchema::Builder::SetDependedOnPrimPath(
    const HdPathDataSourceHandle &dependedOnPrimPath)
{
    _dependedOnPrimPath = dependedOnPrimPath;
    return *this;
}

HdDependencySchema::Builder &
HdDependencySchema::Builder::SetDependedOnDataSourceLocator(
    const HdLocatorDataSourceHandle &dependedOnDataSourceLocator)
{
    _dependedOnDataSourceLocator = dependedOnDataSourceLocator;
    return *this;
}

HdDependencySchema::Builder &
HdDependencySchema::Builder::SetAffectedDataSourceLocator(
    const HdLocatorDataSourceHandle &affectedDataSourceLocator)
{
    _affectedDataSourceLocator = affectedDataSourceLocator;
    return *this;
}

HdContainerDataSourceHandle
HdDependencySchema::Builder::Build()
{
    return HdDependencySchema::BuildRetained(
        _dependedOnPrimPath,
        _dependedOnDataSourceLocator,
        _affectedDataSourceLocator
    );
} 

PXR_NAMESPACE_CLOSE_SCOPE