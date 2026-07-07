#include "pxr/pxr.h"
#include "renderData.h"
#include "valueDescriptor.h"

#include "pxr/base/tf/pyContainerConversions.h"
#include "pxr/base/tf/pyOptional.h"
#include "pxr/base/tf/pyPtrHelpers.h"
#include "pxr/base/tf/pyResultConversions.h"

#include "pxr/usd/usd/pyConversions.h"

#include "pxr/imaging/hdSt/tokens.h"

#include "pxr/external/boost/python.hpp"
#include "pxr/external/boost/python/class.hpp"
#include "pxr/external/boost/python/def.hpp"

#include <optional>

PXR_NAMESPACE_USING_DIRECTIVE

using namespace pxr_boost::python;

// Use an enum to return the MaterialTag values. In C++ these are TfTokens,
// but in HdSt tokens.h they are defined as a limited set of values. We'll
// return these as an enum in python so that it also has this limited set.
//
// Note that I _think_ it is possible for glslfx at least to define new values
// for material tags that are not in the token list. However, these would not
// be used in HdSt to my knowledge, so I'm not supporting them here either. If
// we encounter such a tag (I haven't seen any so far) we'll log a warning and
// return 'other', which is not one of the normal HdSt tags. The other values
// here are taken from Hd tokens.h.
//
// The tags supported by Storm are:
//    defaultMaterialTag : opaque geometry
//    masked : opaque geometry that uses cutout masks (e.g., foliage)
//    displayInOverlay : geometry that should be drawn on top (e.g. guides)
//    translucentToSelection: opaque geometry that allows occluded selection
//                            to show through
//    additive : transparent geometry (cheap OIT solution w/o sorting)
//    translucent: transparent geometry (OIT solution w/ sorted fragment lists)
//    volume : transparent geoometry (raymarched)
//
// Also, these can be blank, so in that case I'm returning defaultMaterialTag.
// I only want to use other to mean, unrecognized tag.

enum class MaterialTag {
    other,
    defaultMaterialTag,
    masked,
    displayInOverlay,
    translucentToSelection,
    additive,
    translucent,
    volume
};

namespace {

    HydraPassthroughValueDescriptor _GetFaceVaryingIndices(
        const HydraPassthroughRenderData::FaceVaryingChannel& self)
    {
        return HydraPassthroughValueDescriptor(self.indices);
    }

    std::vector<std::string> _GetPrimvarNames(
        const HydraPassthroughRenderData::FaceVaryingChannel& self)
    {
        std::vector<std::string> result;
        result.reserve(self.primvars.size());
        for (const auto& primvar : self.primvars) {
            result.push_back(primvar.GetString());
        }
        return result;
    }

    HydraPassthroughValueDescriptor _GetMeshPoints(
        const HydraPassthroughRenderData::MeshData &self)
    {
        return HydraPassthroughValueDescriptor(self.points);
    }

    HydraPassthroughValueDescriptor _GetMeshFaceVertexIndices(
        const HydraPassthroughRenderData::MeshData &self)
    {
        return HydraPassthroughValueDescriptor(VtValue(self.faceVertexIndices));
    }

    HydraPassthroughValueDescriptor _GetMeshPrimitiveParams(
        const HydraPassthroughRenderData::MeshData &self)
    {
        return HydraPassthroughValueDescriptor(self.primitiveParam);
    }

    HydraPassthroughValueDescriptor _GetMeshEdgeIndices(
        const HydraPassthroughRenderData::MeshData &self)
    {
        return HydraPassthroughValueDescriptor(self.edgeIndices);
    }

    HydraPassthroughValueDescriptor _GetMeshInstanceTransforms(
        const HydraPassthroughRenderData::MeshData &self)
    {
        return HydraPassthroughValueDescriptor(VtValue(self.instanceTransforms));
    }

    HydraPassthroughValueDescriptor _GetMeshInstanceIndices(
        const HydraPassthroughRenderData::MeshData &self)
    {
        return HydraPassthroughValueDescriptor(VtValue(self.instanceIndices));
    }

    // Need this to be a function so that it can use TfPySequenceToList
    // return_value_policy.
    const std::vector<GfVec4d> &_GetClippingPlanes(
        const HydraPassthroughRenderData::CameraData &self)
    {
        return self.clipPlanes;
    }

    const std::string _GetLensDistortionType(
        const HydraPassthroughRenderData::CameraData &self)
    {
        return self.lensDistortionType.GetString();
    }

    const MaterialTag _GetMaterialTag(
        const HydraPassthroughRenderData::MaterialData &self)
    {
        std::unordered_map<TfToken, MaterialTag, TfToken::HashFunctor> tagMap = {
            {TfToken(), MaterialTag::defaultMaterialTag},
            {HdStMaterialTagTokens->defaultMaterialTag, MaterialTag::defaultMaterialTag},
            {HdStMaterialTagTokens->masked, MaterialTag::masked},
            {HdStMaterialTagTokens->displayInOverlay, MaterialTag::displayInOverlay},
            {HdStMaterialTagTokens->translucentToSelection, MaterialTag::translucentToSelection},
            {HdStMaterialTagTokens->additive, MaterialTag::additive},
            {HdStMaterialTagTokens->translucent, MaterialTag::translucent},
            {HdStMaterialTagTokens->volume, MaterialTag::volume},
        };
        auto it = tagMap.find(self.tag);
        if (it != tagMap.end()) {
            return it->second;
        }
        TF_WARN("Material %s has unrecognized material tag '%s'",
                self.id.GetText(), self.tag.GetText());
        return MaterialTag::other;
    }

    const VtDictionary &GetMaterialMetadata(
        const HydraPassthroughRenderData::MaterialData &self)
    {
        return self.materialMetadata;
    }

    const std::vector<HydraPassthroughMaterialParam> &_GetMaterialParams(
        const HydraPassthroughRenderData::MaterialData &self)
    {
        return self.materialParams;
    }

    const std::vector<HydraPassthroughTextureDescriptor> &_GetTextureDescriptors(
        const HydraPassthroughRenderData::MaterialData &self)
    {
        return self.textureDescriptors;
    }

    class Primvar {
    public:
        Primvar(const TfToken &name,
                const HydraPassthroughRenderData::PrimvarData & source) :
            name(name.GetString()),
            interpolation(TfEnum::GetDisplayName(source.interpolation)),
            data(source.data)
        {
        }

        std::string name;
        std::string interpolation;
        HydraPassthroughValueDescriptor data;
    };

    std::vector<Primvar> _GetAllPrimvars(
        const HydraPassthroughRenderData::MeshData &self)
    {
        std::vector<Primvar> result;
        result.reserve(self.primvars.size());
        for (const auto &it : self.primvars) {
            result.emplace_back(it.first, it.second);
        }
        return result;
    }

    std::optional<Primvar> _GetPrimvar(
        const HydraPassthroughRenderData::MeshData &self,
        const TfToken &name)
    {
        auto it = self.primvars.find(name);
        if (it != self.primvars.end()) {
            return Primvar(name, it->second);
        }
        return {};
    }

    std::vector<Primvar> _GetAllInstancePrimvars(
        const HydraPassthroughRenderData::MeshData &self)
    {
        std::vector<Primvar> result;
        result.reserve(self.instancePrimvars.size());
        for (const auto &it : self.instancePrimvars) {
            result.emplace_back(it.first, it.second);
        }
        return result;
    }

    std::optional<Primvar> _GetInstancePrimvar(
        const HydraPassthroughRenderData::MeshData &self,
        const TfToken &name)
    {
        auto it = self.instancePrimvars.find(name);
        if (it != self.instancePrimvars.end()) {
            return Primvar(name, it->second);
        }
        return {};
    }

    const std::vector<HydraPassthroughRenderData::FaceVaryingChannel> &_GetFaceVaryingChannels(
        const HydraPassthroughRenderData::MeshData &self)
    {
        return self.faceVaryingChannels;
    }
}

void
wrapRenderData()
{
    using This = HydraPassthroughRenderData;
    scope s = class_<This, TfWeakPtr<This>, noncopyable>("RenderData", no_init)
        .def(TfPyRefAndWeakPtr())
        .def("GetMeshCount", &This::GetMeshCount)
        .def("GetMesh", &This::GetMesh,
             (arg("id")),
             return_internal_reference())
        .def("GetMeshByIndex", &This::GetMeshByIndex,
             (arg("index")),
             return_internal_reference())
        .def("GetCameraCount", &This::GetCameraCount)
        .def("GetCameraByIndex", &This::GetCameraByIndex,
             (arg("index")),
             return_internal_reference())
        .def("GetMaterial", &This::GetMaterial,
             (arg("id")),
             return_internal_reference())
        .def("GetMaterialCount", &This::GetMaterialCount)
        .def("GetMaterialByIndex", &This::GetMaterialByIndex,
             (arg("index")),
             return_internal_reference())

        .def("ExtractRenderDataCopy", &This::ExtractRenderDataCopy)
        .def("ExtractDeindexedRenderDataCopy",
             &This::ExtractDeindexedRenderDataCopy)
        .def("ExtractWeldedRenderDataCopy",
             &This::ExtractWeldedRenderDataCopy)
        ;

    class_<This::RenderData>("RenderDataStruct", no_init)
        .def("GetMeshCount", &This::RenderData::GetMeshCount)
        .def("GetMesh", &This::RenderData::GetMesh,
             (arg("id")),
             return_internal_reference())
        .def("GetMeshByIndex", &This::RenderData::GetMeshByIndex,
             (arg("index")),
             return_internal_reference())
        .def("GetCameraCount", &This::RenderData::GetCameraCount)
        .def("GetCameraByIndex", &This::RenderData::GetCameraByIndex,
             (arg("index")),
             return_internal_reference())
        .def("GetMaterial", &This::RenderData::GetMaterial,
             (arg("id")),
             return_internal_reference())
        .def("GetMaterialCount", &This::RenderData::GetMaterialCount)
        .def("GetMaterialByIndex", &This::RenderData::GetMaterialByIndex,
             (arg("index")),
             return_internal_reference())
        ;

    class_<Primvar>("Primvar", no_init)
        .def_readonly("name", &Primvar::name)
        .def_readonly("interpolation", &Primvar::interpolation)
        .def_readonly("data", &Primvar::data)
        ;
    TfPyOptional::python_optional<Primvar>();

    class_<This::FaceVaryingChannel>("FaceVaryingChannel", no_init)
        .def_readonly("channel", &This::FaceVaryingChannel::channel)
        .def("GetIndices", &_GetFaceVaryingIndices)
        .def("GetPrimvarNames", &_GetPrimvarNames)
        ;

    class_<This::MeshData>("MeshData", no_init)
        .def_readonly("id", &This::MeshData::id)
        .def_readonly("materialId", &This::MeshData::materialId)
        .def_readonly("visible", &This::MeshData::visible)
        .def_readonly("transform", &This::MeshData::transform)
        .def_readonly("faceVaryingChannels", &This::MeshData::faceVaryingChannels)

        .def("GetAllPrimvars", &_GetAllPrimvars,
             return_value_policy<TfPySequenceToList>())
        .def("GetPrimvar", &_GetPrimvar,(arg("name")))
        .def("GetFaceVaryingChannels", &_GetFaceVaryingChannels,
             return_value_policy<TfPySequenceToList>())

        .def("GetPoints", ::_GetMeshPoints)
        .def("GetFaceVertexIndices", ::_GetMeshFaceVertexIndices)
        .def("GetPrimitiveParams", ::_GetMeshPrimitiveParams)
        .def("GetEdgeIndices", ::_GetMeshEdgeIndices)

        .def_readonly("instancerId", &This::MeshData::instancerId)
        .def("GetInstanceTransforms", ::_GetMeshInstanceTransforms)
        .def("GetInstanceIndices", ::_GetMeshInstanceIndices)
        .def("GetAllInstancePrimvars", &_GetAllInstancePrimvars,
             return_value_policy<TfPySequenceToList>())
        .def("GetInstancePrimvar", &_GetInstancePrimvar, (arg("name")))
        ;

    enum_<This::MaterialData::MaterialType>("MaterialType")
        .value("Unknown", This::MaterialData::MaterialType::Unknown)
        .value("PreviewSurface", This::MaterialData::MaterialType::PreviewSurface)
        .value("Volume", This::MaterialData::MaterialType::Volume)
        .value("Other", This::MaterialData::MaterialType::Other)
        ;

    enum_<MaterialTag>("MaterialTag")
        .value("Other", MaterialTag::other)
        .value("Default", MaterialTag::defaultMaterialTag)
        .value("Masked", MaterialTag::masked)
        .value("DisplayInOverlay", MaterialTag::displayInOverlay)
        .value("TranslucentToSelection", MaterialTag::translucentToSelection)
        .value("Additive", MaterialTag::additive)
        .value("Translucent", MaterialTag::translucent)
        .value("Volume", MaterialTag::volume)
        ;

    class_<This::MaterialData>("MaterialData", no_init)
        .def_readonly("id", &This::MaterialData::id)
        .def_readonly("type", &This::MaterialData::type)
        .def("GetMaterialTag", ::_GetMaterialTag)
        .def("GetMaterialMetadata", ::GetMaterialMetadata,
                      return_value_policy<TfPyMapToDictionary>())
        .def("GetMaterialParams", ::_GetMaterialParams,
                      return_value_policy<TfPySequenceToList>())
        .def("GetTextureDescriptors", ::_GetTextureDescriptors,
                      return_value_policy<TfPySequenceToList>())
        ;

    enum_<This::CameraData::Projection>("Projection")
        .value("Perspective", This::CameraData::Projection::Perspective)
        .value("Orthographic", This::CameraData::Projection::Orthographic)
        ;

    enum_<This::CameraData::WindowPolicy>("WindowPolicy")
        .value("MatchVertically", This::CameraData::WindowPolicy::MatchVertically)
        .value("MatchHorizontally", This::CameraData::WindowPolicy::MatchHorizontally)
        .value("Fit", This::CameraData::WindowPolicy::Fit)
        .value("Crop", This::CameraData::WindowPolicy::Crop)
        .value("None", This::CameraData::WindowPolicy::None)
        ;

    class_<This::CameraData>("CameraData", no_init)
        .def_readonly("id", &This::CameraData::id)
        .def_readonly("transform", &This::CameraData::transform)
        .def_readonly("projectionMatrix", &This::CameraData::projectionMatrix)
        .def_readonly("projection", &This::CameraData::projection)
        .def_readonly("horizontalAperture", &This::CameraData::horizontalAperture)
        .def_readonly("verticalAperture", &This::CameraData::verticalAperture)
        .def_readonly("horizontalApertureOffset", &This::CameraData::horizontalApertureOffset)
        .def_readonly("verticalApertureOffset", &This::CameraData::verticalApertureOffset)
        .def_readonly("focalLength", &This::CameraData::focalLength)
        .def_readonly("clippingRange", &This::CameraData::clippingRange)
        .def("GetClipPlanes", ::_GetClippingPlanes,
             return_value_policy<TfPySequenceToList>())
        .def_readonly("fStop", &This::CameraData::fStop)
        .def_readonly("focusDistance", &This::CameraData::focusDistance)
        .def_readonly("focusOn", &This::CameraData::focusOn)
        .def_readonly("dofAspect", &This::CameraData::dofAspect)
        .def_readonly("shutterOpen", &This::CameraData::shutterOpen)
        .def_readonly("shutterClose", &This::CameraData::shutterClose)
        .def_readonly("linearExposureScale", &This::CameraData::linearExposureScale)
        .def("GetLensDistortionType", ::_GetLensDistortionType)
        .def_readonly("lensDistortionK1", &This::CameraData::lensDistortionK1)
        .def_readonly("lensDistortionK2", &This::CameraData::lensDistortionK2)
        .def_readonly("lensDistortionCenter", &This::CameraData::lensDistortionCenter)
        .def_readonly("lensDistortionAnaSq", &This::CameraData::lensDistortionAnaSq)
        .def_readonly("lensDistortionAsym", &This::CameraData::lensDistortionAsym)
        .def_readonly("lensDistortionScale", &This::CameraData::lensDistortionScale)
        .def_readonly("lensDistortionIor", &This::CameraData::lensDistortionIor)
        .def_readonly("splitDiopterCount", &This::CameraData::splitDiopterCount)
        .def_readonly("splitDiopterAngle", &This::CameraData::splitDiopterAngle)
        .def_readonly("splitDiopterOffset1", &This::CameraData::splitDiopterOffset1)
        .def_readonly("splitDiopterWidth1", &This::CameraData::splitDiopterWidth1)
        .def_readonly("splitDiopterFocusDistance1", &This::CameraData::splitDiopterFocusDistance1)
        .def_readonly("splitDiopterOffset2", &This::CameraData::splitDiopterOffset2)
        .def_readonly("splitDiopterWidth2", &This::CameraData::splitDiopterWidth2)
        .def_readonly("splitDiopterFocusDistance2", &This::CameraData::splitDiopterFocusDistance2)
        .def_readonly("windowPolicy", &This::CameraData::windowPolicy)
        ;
}

