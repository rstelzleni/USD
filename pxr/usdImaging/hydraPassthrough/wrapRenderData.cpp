#include "pxr/pxr.h"
#include "renderData.h"
#include "valueDescriptor.h"

#include "pxr/base/tf/pyContainerConversions.h"
#include "pxr/base/tf/pyOptional.h"
#include "pxr/base/tf/pyPtrHelpers.h"
#include "pxr/base/tf/pyResultConversions.h"

#include "pxr/usd/usd/pyConversions.h"

#include "pxr/external/boost/python.hpp"
#include "pxr/external/boost/python/class.hpp"
#include "pxr/external/boost/python/def.hpp"

#include <optional>

PXR_NAMESPACE_USING_DIRECTIVE

using namespace pxr_boost::python;

namespace {
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

    const std::string _GetMaterialTag(
        const HydraPassthroughRenderData::MaterialData &self)
    {
        return self.tag.GetString();
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
                const HydraPassthroughRenderData::PrimvarSource & source) :
            name(name.GetString()),
            interpolation(TfEnum::GetDisplayName(source.interpolation)),
            role(source.role.GetString()),
            data(source.updatedData.IsEmpty() ?
                        source.data : source.updatedData)
        {}

        std::string name;
        std::string interpolation;
        std::string role;
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
        .def_readonly("role", &Primvar::role)
        ;
    TfPyOptional::python_optional<Primvar>();

    class_<This::MeshData>("MeshData", no_init)
        .def_readonly("id", &This::MeshData::id)
        .def_readonly("materialId", &This::MeshData::materialId)
        .def_readonly("visible", &This::MeshData::visible)
        .def_readonly("transform", &This::MeshData::transform)
        .def_readonly("points", &This::MeshData::points)
        .def_readonly("faceVertexIndices", &This::MeshData::faceVertexIndices)

        .def_readonly("triangleOriginalFaceIndices", &This::MeshData::triangleOriginalFaceIndices)
        .def_readonly("triangleEdgeIndices", &This::MeshData::triangleEdgeIndices)

        .def("GetAllPrimvars", &_GetAllPrimvars,
             return_value_policy<TfPySequenceToList>())
        .def("GetPrimvar", &_GetPrimvar,(arg("name")))
        ;

    enum_<This::MaterialData::MaterialType>("MaterialType")
        .value("Unknown", This::MaterialData::MaterialType::Unknown)
        .value("PreviewSurface", This::MaterialData::MaterialType::PreviewSurface)
        .value("Volume", This::MaterialData::MaterialType::Volume)
        .value("Other", This::MaterialData::MaterialType::Other)
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

