#include "pxr/pxr.h"
#include "materialParam.h"

#include "pxr/base/tf/pyContainerConversions.h"
#include "pxr/base/tf/pyPtrHelpers.h"
#include "pxr/base/tf/pyResultConversions.h"

#include "pxr/external/boost/python.hpp"
#include "pxr/external/boost/python/class.hpp"
#include "pxr/external/boost/python/def.hpp"

PXR_NAMESPACE_USING_DIRECTIVE

using namespace pxr_boost::python;

namespace
{
    const std::vector<std::string> &_GetSamplerCoords(
        const HydraPassthroughMaterialParam &self)
    {
        return self.samplerCoords;
    }
}

void
wrapMaterialParam()
{
    using This = HydraPassthroughMaterialParam;
    scope s = class_<This >("MaterialParam", no_init)
        .def("__str__", &This::ToString)
        .def_readonly("paramType", &This::paramType)
        .def_readonly("name", &This::name)
        .def_readonly("fallbackValue", &This::fallbackValue)
        .def("GetSamplerCoords", ::_GetSamplerCoords,
            return_value_policy<TfPySequenceToList>())
        .def_readonly("textureType", &This::textureType)
        .def_readonly("swizzle", &This::swizzle)
        .def_readonly("isPremultiplied", &This::isPremultiplied)
        .def_readonly("arrayOfTexturesSize", &This::arrayOfTexturesSize)
        ;

    enum_<This::ParamType>("ParamType")
        .value("Fallback", This::ParamType::Fallback)
        .value("Texture", This::ParamType::Texture)
        .value("PrimvarRedirect", This::ParamType::PrimvarRedirect)
        .value("FieldRedirect", This::ParamType::FieldRedirect)
        .value("AdditionalPrimvar", This::ParamType::AdditionalPrimvar)
        .value("Transform2d", This::ParamType::Transform2d)
        ;

    enum_<This::TextureType>("TextureType")
        .value("NoTexture", This::TextureType::None)
        .value("Uv", This::TextureType::Uv)
        .value("Field", This::TextureType::Field)
        .value("Ptex", This::TextureType::Ptex)
        .value("Udim", This::TextureType::Udim)
        ;
}

