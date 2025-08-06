//
// Copyright 2020 Pixar
//
// Licensed under the terms set forth in the LICENSE.txt file available at
// https://openusd.org/license.
//
#include "mesh.h"

#include <iostream>

PXR_NAMESPACE_OPEN_SCOPE

HdTinyMesh::HdTinyMesh(SdfPath const &id) : HdMesh(id) {}

HdDirtyBits HdTinyMesh::GetInitialDirtyBitsMask() const {
  return HdChangeTracker::Clean | HdChangeTracker::DirtyTransform;
}

HdDirtyBits HdTinyMesh::_PropagateDirtyBits(HdDirtyBits bits) const {
  return bits;
}

void HdTinyMesh::_InitRepr(TfToken const &reprToken, HdDirtyBits *dirtyBits) {
    std::cout << "=> InitRepr for Tiny Mesh id=" << GetId() << " reprToken="
                << reprToken << " dirtyBits=" << *dirtyBits << std::endl;
    
    // Initialize the representation with a default Repr.
    // An rprim owns a shared data object, and it also has potentially 
    // multiple reprs. Each repr can create one or more draw items,  which
    // can share that data. These need to be initialized in _InitRepr using
    // the protected members _reprs and _sharedData.
    _ReprVector::iterator it = std::find_if(_reprs.begin(), _reprs.end(),
                                            _ReprComparator(reprToken));
    bool isNew = it == _reprs.end();

    if (isNew) {
        std::cout << "Creating new repr for Tiny Mesh id=" << GetId()
                  << " reprToken=" << reprToken << std::endl;

        // add new repr
        _reprs.emplace_back(reprToken, std::make_shared<HdRepr>());
        HdReprSharedPtr &repr = _reprs.back().second;

        // set dirty bit to say we need to sync a new repr (buffer array
        // ranges may change)
        *dirtyBits |= HdChangeTracker::DirtyRepr;

        HdRepr::DrawItemUniquePtr drawItem =
            std::make_unique<HdDrawItem>(&_sharedData);
        //HdDrawingCoord *drawingCoord = drawItem->GetDrawingCoord();
        repr->AddDrawItem(std::move(drawItem));

    } else {
        std::cout << "Repr already exists for Tiny Mesh id=" << GetId()
                  << " reprToken=" << reprToken << std::endl;
    }
}


void HdTinyMesh::Sync(HdSceneDelegate *sceneDelegate,
                      HdRenderParam *renderParam, HdDirtyBits *dirtyBits,
                      TfToken const &reprToken) {
  std::cout << "* (multithreaded) Sync Tiny Mesh id=" << GetId() << std::endl;
}

PXR_NAMESPACE_CLOSE_SCOPE
