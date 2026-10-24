#ifndef USD_IMAGING_HYDRA_PASSTHROUGH_IMPLICIT_SURFACE_SCENE_INDEX_PLUGIN_H
#define USD_IMAGING_HYDRA_PASSTHROUGH_IMPLICIT_SURFACE_SCENE_INDEX_PLUGIN_H

#include "pxr/pxr.h"
#include "pxr/imaging/hd/sceneIndexPlugin.h"

PXR_NAMESPACE_OPEN_SCOPE

/// \class HydraPassthrough_ImplicitSurfaceSceneIndexPlugin
///
/// Converts implicit surfaces (cube, sphere, cone, cylinder, capsule and
/// plane) to meshes, since mesh is the only rprim type we support.
///
/// Storm and Embree register their own copies of this plugin; scene index
/// plugins only load for the renderer they name, so we need ours too.
///
class HydraPassthrough_ImplicitSurfaceSceneIndexPlugin : public HdSceneIndexPlugin
{
public:
    HydraPassthrough_ImplicitSurfaceSceneIndexPlugin();

protected:
    HdSceneIndexBaseRefPtr _AppendSceneIndex(
        const HdSceneIndexBaseRefPtr &inputScene,
        const HdContainerDataSourceHandle &inputArgs) override;
};

PXR_NAMESPACE_CLOSE_SCOPE

#endif // USD_IMAGING_HYDRA_PASSTHROUGH_IMPLICIT_SURFACE_SCENE_INDEX_PLUGIN_H
