//
// Copyright 2025 Pixar
//
// Licensed under the terms set forth in the LICENSE.txt file available at
// https://openusd.org/license.

#ifndef PXR_IMAGING_HD_ST_UNBOUND_MATERIAL_OVERRIDING_SCENE_INDEX_PLUGIN_H
#define PXR_IMAGING_HD_ST_UNBOUND_MATERIAL_OVERRIDING_SCENE_INDEX_PLUGIN_H

#include "pxr/pxr.h"
#include "pxr/imaging/hdSt/api.h"
#include "pxr/imaging/hd/sceneIndexPlugin.h"

PXR_NAMESPACE_OPEN_SCOPE

/// \class HdSt_UnboundMaterialOverridingSceneIndexPlugin
///
/// Plugin adds a scene index that nullifies the prim data source for
/// material prims that are not bound.
///
class HdSt_UnboundMaterialOverridingSceneIndexPlugin : public HdSceneIndexPlugin
{
public:
    HdSt_UnboundMaterialOverridingSceneIndexPlugin();

protected:
    HdSceneIndexBaseRefPtr _AppendSceneIndex(
        const HdSceneIndexBaseRefPtr &inputScene,
        const HdContainerDataSourceHandle &inputArgs) override;
};

PXR_NAMESPACE_CLOSE_SCOPE

#endif // PXR_IMAGING_HD_ST_UNBOUND_MATERIAL_OVERRIDING_SCENE_INDEX_PLUGIN_H
