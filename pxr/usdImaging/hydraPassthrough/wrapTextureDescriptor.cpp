#include "pxr/pxr.h"
#include "textureDescriptor.h"

#include "pxr/base/tf/pyContainerConversions.h"
#include "pxr/base/tf/pyPtrHelpers.h"
#include "pxr/base/tf/pyResultConversions.h"

#include "pxr/external/boost/python.hpp"
#include "pxr/external/boost/python/class.hpp"
#include "pxr/external/boost/python/def.hpp"

PXR_NAMESPACE_USING_DIRECTIVE

using namespace pxr_boost::python;

// Duplicate these enums from Hd so we can wrap them in this interface. They
// are used indirectly through HdSamplerParameters, and originally defined
// in Hd enums.h and types.h.
//
// If we wrapped the Hd types there's always a chance someone else will also
// add bindings for those types, resulting in runtime failures.
//
// Note that in the texture descriptor we use Hd types directly, so in C++
// you can use Hd enums. These exist only for the python api.

// Wrapping behavior
enum class HydraPassthroughWrap
{
    Clamp,
    Repeat,
    Black,
    Mirror,

    NoOpinion,

    // No need to duplicate these
//    HdWrapLegacyNoOpinionFallbackRepeat, // deprecated
//    HdWrapUseMetadata = HdWrapNoOpinion, // deprecated alias
//    HdWrapLegacy = HdWrapLegacyNoOpinionFallbackRepeat // deprecated alias
};

// Min filtering behavior
enum class HydraPassthroughMinFilter 
{
    Nearest,
    Linear,
    NearestMipmapNearest,
    LinearMipmapNearest,
    NearestMipmapLinear,
    LinearMipmapLinear,
};

/// Mag filtering behavior
enum class HydraPassthroughMagFilter 
{
    Nearest,
    Linear,
};

// Border color options
enum class HydraPassthroughBorderColor 
{
    TransparentBlack,
    OpaqueBlack,
    OpaqueWhite,
};

// Compare function options
enum class HydraPassthroughCompareFunction
{
    Never,
    Less,
    Equal,
    LEqual,
    Greater,
    NotEqual,
    GEqual,
    Always,
};

namespace
{
    HydraPassthroughWrap _ConvertWrap(HdWrap w)
    {
        switch (w) {
        case HdWrapClamp: return HydraPassthroughWrap::Clamp;
        case HdWrapRepeat: return HydraPassthroughWrap::Repeat;
        case HdWrapBlack: return HydraPassthroughWrap::Black;
        case HdWrapMirror: return HydraPassthroughWrap::Mirror;
        case HdWrapNoOpinion: return HydraPassthroughWrap::NoOpinion;
        default:
            TF_CODING_ERROR("Unknown HdWrap value %d", (int)w);
            return HydraPassthroughWrap::NoOpinion;
        }
    }

    HydraPassthroughWrap _GetWrapS(
        const HydraPassthroughTextureDescriptor &self)
    {
        return _ConvertWrap(self.samplerParameters.wrapS);
    }

    HydraPassthroughWrap _GetWrapT(
        const HydraPassthroughTextureDescriptor &self)
    {
        return _ConvertWrap(self.samplerParameters.wrapT);
    }

    HydraPassthroughWrap _GetWrapR(
        const HydraPassthroughTextureDescriptor &self)
    {
        return _ConvertWrap(self.samplerParameters.wrapT);
    }

    HydraPassthroughMinFilter _GetMinFilter(
        const HydraPassthroughTextureDescriptor &self)
    {
        switch (self.samplerParameters.minFilter) {
        case HdMinFilterNearest: return HydraPassthroughMinFilter::Nearest;
        case HdMinFilterLinear: return HydraPassthroughMinFilter::Linear;
        case HdMinFilterNearestMipmapNearest:
            return HydraPassthroughMinFilter::NearestMipmapNearest;
        case HdMinFilterLinearMipmapNearest:
            return HydraPassthroughMinFilter::LinearMipmapNearest;
        case HdMinFilterNearestMipmapLinear:
            return HydraPassthroughMinFilter::NearestMipmapLinear;
        case HdMinFilterLinearMipmapLinear:
            return HydraPassthroughMinFilter::LinearMipmapLinear;
        default:
            TF_CODING_ERROR("Unknown HdMinFilter value %d",
                (int)self.samplerParameters.minFilter);
            return HydraPassthroughMinFilter::Nearest;
        }
    }

    HydraPassthroughMagFilter _GetMagFilter(
        const HydraPassthroughTextureDescriptor &self)
    {
        switch (self.samplerParameters.magFilter) {
        case HdMagFilterNearest: return HydraPassthroughMagFilter::Nearest;
        case HdMagFilterLinear: return HydraPassthroughMagFilter::Linear;
        default:
            TF_CODING_ERROR("Unknown HdMagFilter value %d",
                (int)self.samplerParameters.magFilter);
            return HydraPassthroughMagFilter::Nearest;
        }
    }

    HydraPassthroughBorderColor _GetBorderColor(
        const HydraPassthroughTextureDescriptor &self)
    {
        switch (self.samplerParameters.borderColor) {
        case HdBorderColorTransparentBlack:
            return HydraPassthroughBorderColor::TransparentBlack;
        case HdBorderColorOpaqueBlack:
            return HydraPassthroughBorderColor::OpaqueBlack;
        case HdBorderColorOpaqueWhite:
            return HydraPassthroughBorderColor::OpaqueWhite;
        default:
            TF_CODING_ERROR("Unknown HdBorderColor value %d",
                (int)self.samplerParameters.borderColor);
            return HydraPassthroughBorderColor::TransparentBlack;
        }
    }

    HydraPassthroughCompareFunction _GetCompareFunction(
        const HydraPassthroughTextureDescriptor &self)
    {
        switch (self.samplerParameters.compareFunction) {
        case HdCmpFuncNever: return HydraPassthroughCompareFunction::Never;
        case HdCmpFuncLess: return HydraPassthroughCompareFunction::Less;
        case HdCmpFuncEqual: return HydraPassthroughCompareFunction::Equal;
        case HdCmpFuncLEqual: return HydraPassthroughCompareFunction::LEqual;
        case HdCmpFuncGreater: return HydraPassthroughCompareFunction::Greater;
        case HdCmpFuncNotEqual:
            return HydraPassthroughCompareFunction::NotEqual;
        case HdCmpFuncGEqual:
            return HydraPassthroughCompareFunction::GEqual;
        case HdCmpFuncAlways:
            return HydraPassthroughCompareFunction::Always;
        default:
            TF_CODING_ERROR("Unknown HdCompareFunction value %d",
                (int)self.samplerParameters.compareFunction);
            return HydraPassthroughCompareFunction::Never;
        }
    }

    bool _GetEnableCompare(
        const HydraPassthroughTextureDescriptor &self)
    {
        return self.samplerParameters.enableCompare;
    }

    int _GetMaxAnisotropy(
        const HydraPassthroughTextureDescriptor &self)
    {
        return self.samplerParameters.maxAnisotropy;
    }
}

void
wrapTextureDescriptor()
{
    using This = HydraPassthroughTextureDescriptor;

    scope s = class_<This>("TextureDescriptor", no_init)
        .def_readonly("name", &This::name)
        .def_readonly("filePath", &This::filePath)
        .def_readonly("type", &This::type)
        .def_readonly("sourceColorSpace", &This::sourceColorSpace)
        .def_readonly("memoryRequest", &This::memoryRequest)
        .def("GetWrapS", ::_GetWrapS)
        .def("GetWrapT", ::_GetWrapT)
        .def("GetWrapR", ::_GetWrapR)
        .def("GetMinFilter", ::_GetMinFilter)
        .def("GetMagFilter", ::_GetMagFilter)
        .def("GetBorderColor", ::_GetBorderColor)
        .def("GetEnableCompare", ::_GetEnableCompare)
        .def("GetCompareFunction", ::_GetCompareFunction)
        .def("GetMaxAnisotropy", ::_GetMaxAnisotropy)
        ;

    enum_<HydraPassthroughWrap>("Wrap")
        .value("Clamp", HydraPassthroughWrap::Clamp)
        .value("Repeat", HydraPassthroughWrap::Repeat)
        .value("Black", HydraPassthroughWrap::Black)
        .value("Mirror", HydraPassthroughWrap::Mirror)
        .value("NoOpinion", HydraPassthroughWrap::NoOpinion)
        ;

    enum_<HydraPassthroughMinFilter>("MinFilter")
        .value("Nearest", HydraPassthroughMinFilter::Nearest)
        .value("Linear", HydraPassthroughMinFilter::Linear)
        .value("NearestMipmapNearest", HydraPassthroughMinFilter::NearestMipmapNearest)
        .value("LinearMipmapNearest", HydraPassthroughMinFilter::LinearMipmapNearest)
        .value("NearestMipmapLinear", HydraPassthroughMinFilter::NearestMipmapLinear)
        .value("LinearMipmapLinear", HydraPassthroughMinFilter::LinearMipmapLinear)
        ;

    enum_<HydraPassthroughMagFilter>("MagFilter")
        .value("Nearest", HydraPassthroughMagFilter::Nearest)
        .value("Linear", HydraPassthroughMagFilter::Linear)
        ;

    enum_<HydraPassthroughBorderColor>("BorderColor")
        .value("TransparentBlack", HydraPassthroughBorderColor::TransparentBlack)
        .value("OpaqueBlack", HydraPassthroughBorderColor::OpaqueBlack)
        .value("OpaqueWhite", HydraPassthroughBorderColor::OpaqueWhite)
        ;

    enum_<HydraPassthroughCompareFunction>("CompareFunction")
        .value("Never", HydraPassthroughCompareFunction::Never)
        .value("Less", HydraPassthroughCompareFunction::Less)
        .value("Equal", HydraPassthroughCompareFunction::Equal)
        .value("LEqual", HydraPassthroughCompareFunction::LEqual)
        .value("Greater", HydraPassthroughCompareFunction::Greater)
        .value("NotEqual", HydraPassthroughCompareFunction::NotEqual)
        .value("GEqual", HydraPassthroughCompareFunction::GEqual)
        .value("Always", HydraPassthroughCompareFunction::Always)
        ;
}

