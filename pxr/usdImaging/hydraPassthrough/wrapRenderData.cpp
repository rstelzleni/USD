#include "pxr/pxr.h"
#include "renderData.h"

#include "pxr/base/tf/pyPtrHelpers.h"

#include "pxr/external/boost/python.hpp"
#include "pxr/external/boost/python/class.hpp"
#include "pxr/external/boost/python/def.hpp"

PXR_NAMESPACE_USING_DIRECTIVE

using namespace pxr_boost::python;

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
}

