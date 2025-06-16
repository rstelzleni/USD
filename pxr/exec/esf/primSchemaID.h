//
// Copyright 2025 Pixar
//
// Licensed under the terms set forth in the LICENSE.txt file available at
// https://openusd.org/license.
//
#ifndef PXR_EXEC_ESF_PRIM_SCHEMA_ID_H
#define PXR_EXEC_ESF_PRIM_SCHEMA_ID_H

/// \file

#include "pxr/pxr.h"

#include "pxr/exec/esf/api.h"

PXR_NAMESPACE_OPEN_SCOPE

/// An opaque type that can be used to identify the configuration of typed
/// and applied schemas for a prim.
///
class EsfPrimSchemaID {
  public:
    /// Only null IDs can be constructed publicly.
    EsfPrimSchemaID() : _id(nullptr) {}

    bool operator==(EsfPrimSchemaID id) const {
        return _id == id._id;
    }

    bool operator!=(EsfPrimSchemaID id) const {
        return !(*this == id);
    }

    template <class HashState>
    friend void TfHashAppend(HashState &h, const EsfPrimSchemaID id) {
        h.Append(id._id);
    }

  private:
    // Derived classes can construct a EsfPrimSchemaID by calling
    // EsfPrimInterface::CreateEsfPrimSchemaID.
    //
    friend class EsfPrimInterface;
    explicit EsfPrimSchemaID(const void *const id)
        : _id(id)
    {
    }

  private:
    const void *_id;
};

PXR_NAMESPACE_CLOSE_SCOPE

#endif
