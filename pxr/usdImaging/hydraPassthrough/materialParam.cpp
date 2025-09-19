#include "pxr/usdImaging/hydraPassthrough/materialParam.h"

#include "pxr/base/tf/hash.h"

PXR_NAMESPACE_OPEN_SCOPE

HydraPassthroughMaterialParam::HydraPassthroughMaterialParam()
    : paramType(ParamType::Fallback)
    , name()
    , fallbackValue()
    , samplerCoords()
    , textureType(TextureType::Uv)
    , swizzle()
    , isPremultiplied(false)
    , arrayOfTexturesSize(0)
{
}

HydraPassthroughMaterialParam::HydraPassthroughMaterialParam(
        ParamType paramType,
        std::string const& name, 
        VtValue const& fallbackValue,
        std::vector<std::string> const& samplerCoords,
        TextureType textureType,
        std::string const& swizzle,
        bool const isPremultiplied,
        size_t const arrayOfTexturesSize)
    : paramType(paramType)
    , name(name)
    , fallbackValue(fallbackValue)
    , samplerCoords(samplerCoords)
    , textureType(textureType)
    , swizzle(swizzle)
    , isPremultiplied(isPremultiplied)
    , arrayOfTexturesSize(arrayOfTexturesSize)
{
}

size_t
HydraPassthroughMaterialParam::ComputeHash(
        std::vector<HydraPassthroughMaterialParam> const &params)
{
    size_t hash = 0;
    for (HydraPassthroughMaterialParam const& param : params) {
        hash = TfHash::Combine(
            hash,
            param.paramType,
            param.name,
            param.samplerCoords,
            param.textureType,
            param.swizzle,
            param.isPremultiplied,
            param.arrayOfTexturesSize
        );
    }
    return hash;
}

HdTupleType
HydraPassthroughMaterialParam::GetTupleType() const
{
    return HdGetValueTupleType(fallbackValue);
}

PXR_NAMESPACE_CLOSE_SCOPE

