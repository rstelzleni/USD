#ifndef USD_IMAGING_HYDRA_PASSTHROUGH_UNBOUND_MATERIAL_PRUNING_SCENE_INDEX_PLUGIN_H
#define USD_IMAGING_HYDRA_PASSTHROUGH_UNBOUND_MATERIAL_PRUNING_SCENE_INDEX_PLUGIN_H

#include "pxr/pxr.h"
#include "pxr/imaging/hd/sceneIndexPlugin.h"

PXR_NAMESPACE_OPEN_SCOPE

/// \class HydraPassthrough_UnboundMaterialPruningSceneIndexPlugin
///
/// Prunes materials that no prim binds, so they aren't synced or included
/// in the render data. Matches Storm's plugin, which only loads for Storm.
///
class HydraPassthrough_UnboundMaterialPruningSceneIndexPlugin
    : public HdSceneIndexPlugin
{
public:
    HydraPassthrough_UnboundMaterialPruningSceneIndexPlugin();

protected:
    HdSceneIndexBaseRefPtr _AppendSceneIndex(
        const HdSceneIndexBaseRefPtr &inputScene,
        const HdContainerDataSourceHandle &inputArgs) override;
};

PXR_NAMESPACE_CLOSE_SCOPE

#endif // USD_IMAGING_HYDRA_PASSTHROUGH_UNBOUND_MATERIAL_PRUNING_SCENE_INDEX_PLUGIN_H
