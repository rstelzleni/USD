#ifndef HYDRA_PASSTHROUGH_MATERIAL_PARAM_H
#define HYDRA_PASSTHROUGH_MATERIAL_PARAM_H

#include "pxr/pxr.h"
#include "pxr/imaging/hd/types.h"
#include "pxr/usd/sdf/path.h"
#include "pxr/base/vt/value.h"
#include "pxr/base/tf/token.h"

#include <string>
#include <vector>

PXR_NAMESPACE_OPEN_SCOPE


/// Represents a material parameter.
class HydraPassthroughMaterialParam {
public:
    // Hash type
    typedef size_t ID;

    // Indicates the kind of material parameter.
    // XXX RYANS and maybe the names to remove the Redirect parts.
    enum class ParamType {
        // This is a shader specified fallback value that is
        // not connected to either a primvar or texture.
        Fallback,
        // This is a parameter that is connected to a texture.
        Texture,
        // Corresponds to a primvar reader shading node.
        PrimvarRedirect,
        // Corresponds to a field reader shading node.
        FieldRedirect,
        // Additional primvar needed by material. One that is not connected to
        // a input parameter (ParamTypePrimvar).
        AdditionalPrimvar,
        // This is a parameter that is connected to a transform2d node
        Transform2d
    };

    // Texture types to track. 
    //
    // Not all these are supported, but we want to know if they're present
    // in the data, and the client can handle that as needed.
    enum class TextureType {
        None,
        Uv,     // Standard 2D texture accessed by UV coordinates
        Field,  // 3D texture accessed by field coordinates
        Ptex,   // Ptex texture
        Udim    // UDIM texture set accessed by UV coordinates
    };

    HydraPassthroughMaterialParam();

    HydraPassthroughMaterialParam(ParamType paramType,
                                  std::string const& name, 
                                  VtValue const& fallbackValue,
                                  std::vector<std::string> const& samplerCoords=std::vector<std::string>(),
                                  TextureType textureType=TextureType::Uv,
                                  std::string const& swizzle=std::string(),
                                  bool const isPremultiplied=false,
                                  size_t const arrayOfTexturesSize=0);

    /// Computes a hash for all parameters using structural information
    /// (name, texture type, primvar names) but not the fallback value.
    static ID ComputeHash(
            std::vector<HydraPassthroughMaterialParam> const &shaders);

    // Gets the type and size of the parameter's value. Example results
    // might be like
    // tuple.type = HdTypeFloat, tuple.count = 1
    // tuple.type = HdTypeFloatVec3, tuple.count = 32
    //
    // See Hd/types.h for type values and helper functions
    HdTupleType GetTupleType() const;

    std::string ToString() const;

    ParamType paramType { ParamType::Fallback };
    std::string name;
    VtValue fallbackValue;
    std::vector<std::string> samplerCoords;
    TextureType textureType { TextureType::None };

    // Swizzle looks like xyzw, rgba, etc.
    std::string swizzle;
    bool isPremultiplied { false };

    // In our current implementation this is never set. In the HdSt code this
    // value is used to indicate if the texture is an array of textures.
    //
    // If paramType is ParamType::Texture, this indicates both if the textures
    // should be bound as an array of textures and the size of the array. If
    // arrayOfTexturesSize is 0, then do not bind as an array of textures, but
    // rather a single texture (whereas arrayOfTexturesSize = 1 indicates an
    // array of textures of size 1).
    size_t arrayOfTexturesSize { 0 };
};

PXR_NAMESPACE_CLOSE_SCOPE

#endif // HYDRA_PASSTHROUGH_MATERIAL_PARAM_H
