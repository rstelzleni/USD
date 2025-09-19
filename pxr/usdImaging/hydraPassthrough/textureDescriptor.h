#ifndef USD_IMAGING_HYDRA_PASSTHROUGH_TEXTURE_PARAM_H
#define USD_IMAGING_HYDRA_PASSTHROUGH_TEXTURE_PARAM_H

#include "pxr/pxr.h"
#include "pxr/imaging/hd/types.h"
#include "pxr/usdImaging/hydraPassthrough/materialParam.h"

#include <string>

PXR_NAMESPACE_OPEN_SCOPE


/// Texture descriptor for textures used by a material
///
/// Note that a material may have several of these referring to the same
/// texture file, but with a different name and potentially other parameters.
/// This is the case if, for instance, a texture is connected to diffuseColor
/// and opacity.
struct HydraPassthroughTextureDescriptor
{
    // A unique ID for the texture
    std::string name;
    std::string filePath;
    HydraPassthroughMaterialParam::TextureType type;
    std::string sourceColorSpace;
    HdSamplerParameters samplerParameters;
    // Memory request size in bytes.
    size_t memoryRequest{0};

    std::string ToString() const;
};

PXR_NAMESPACE_CLOSE_SCOPE

#endif // USD_IMAGING_HYDRA_PASSTHROUGH_TEXTURE_PARAM_H
