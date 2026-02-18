#ifndef USD_IMAGING_HYDRA_PASSTHROUGH_MATERIAL_H
#define USD_IMAGING_HYDRA_PASSTHROUGH_MATERIAL_H

#include "pxr/pxr.h"

#include "pxr/usdImaging/hydraPassthrough/materialParam.h"
#include "pxr/usdImaging/hydraPassthrough/renderData.h"
#include "pxr/usdImaging/hydraPassthrough/renderParam.h"
#include "pxr/imaging/hd/material.h"

PXR_NAMESPACE_OPEN_SCOPE

using HioGlslfxSharedPtr = std::shared_ptr<class HioGlslfx>;

/// \class HydraPassthroughMaterial
///
/// Material sprim for syncing material properties.
///
class HydraPassthroughMaterial final : public HdMaterial {
public:
    HydraPassthroughMaterial(SdfPath const& id);
    ~HydraPassthroughMaterial() override;

    /// Synchronizes state from the delegate to this object.
    void Sync(HdSceneDelegate *sceneDelegate,
              HdRenderParam   *renderParam,
              HdDirtyBits     *dirtyBits) override;

    /// Tell the change tracker we need to sync everything.
    HdDirtyBits GetInitialDirtyBitsMask() const override;

    /// True if any material param has a texture that's ptex
    bool HasPtex() const;

    std::string ToString() const;

private:

    // Read the material network and populate member variables
    bool _ProcessMaterialNetwork(
        HdSceneDelegate *sceneDelegate,
        HydraPassthroughGlslfxCache &glslfxCache);

    // Export material information into the render data
    void _AddMaterialToOutput(
        HdRenderParam   *renderParam);

    HioGlslfxSharedPtr _surfaceGfx;
    size_t _surfaceGfxHash{0};

    bool _isPreviewSurface{false};
    bool _isVolumeMaterial{false};

    std::string _displacementSource; // unused at the moment, but we'll want it
                                     // for materials with displacement
    TfToken _materialTag;
    VtDictionary _materialMetadata;
    std::vector<HydraPassthroughMaterialParam> _materialParams;
    std::vector<HydraPassthroughTextureDescriptor> _textureDescriptors;
};

PXR_NAMESPACE_CLOSE_SCOPE

#endif // USD_IMAGING_HYDRAD_PASSHTROUGH_MATERIAL_H
