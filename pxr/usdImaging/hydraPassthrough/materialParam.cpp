#include "pxr/usdImaging/hydraPassthrough/materialParam.h"

#include "pxr/base/tf/hash.h"

#include <sstream>

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

std::string
HydraPassthroughMaterialParam::ToString() const
{
    std::stringstream ss;
    ss << "HydraPassthroughMaterialParam {" << std::endl;
    ss << "  paramType: " << (int)paramType << std::endl;
    ss << "  name: " << name << std::endl;
    ss << "  fallbackValue: " << TfStringify(fallbackValue) ;
    ss << std::endl;
    ss << "  samplerCoords: ";
    for (auto const& sc : samplerCoords) {
        ss << sc << " ";
    }
    ss << std::endl;
    ss << "  textureType: " << (int)textureType << std::endl;
    ss << "  swizzle: " << swizzle << std::endl;
    ss << "  isPremultiplied: " << (isPremultiplied ? "true" : "false") << std::endl;
    ss << "  arrayOfTexturesSize: " << arrayOfTexturesSize << std::endl;
    ss << "}";
    return ss.str();
}

PXR_NAMESPACE_CLOSE_SCOPE

