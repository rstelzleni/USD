#include "pxr/pxr.h"
#include "renderData.h"

#include "pxr/base/tf/pyContainerConversions.h"
#include "pxr/base/tf/pyPtrHelpers.h"
#include "pxr/base/tf/pyResultConversions.h"

#include "pxr/external/boost/python.hpp"
#include "pxr/external/boost/python/class.hpp"
#include "pxr/external/boost/python/def.hpp"

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
        ;

    class_<This::MeshData>("MeshData", no_init)
        .def_readonly("id", &This::MeshData::id)
        .def_readonly("visible", &This::MeshData::visible)
        .def_readonly("transform", &This::MeshData::transform)
        .def_readonly("points", &This::MeshData::points)
        .def_readonly("faceVertexIndices", &This::MeshData::faceVertexIndices)

        .def_readonly("triangleOriginalFaceIndices", &This::MeshData::triangleOriginalFaceIndices)
        .def_readonly("triangleEdgeIndices", &This::MeshData::triangleEdgeIndices)
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
        .def_readonly("lensDistortionType", &This::CameraData::lensDistortionType)
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

