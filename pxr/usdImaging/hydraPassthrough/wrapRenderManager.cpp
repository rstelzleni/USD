#include "pxr/pxr.h"
#include "renderManager.h"

#include "pxr/external/boost/python.hpp"
#include "pxr/external/boost/python/class.hpp"
#include "pxr/external/boost/python/def.hpp"

PXR_NAMESPACE_USING_DIRECTIVE

using namespace pxr_boost::python;

void
wrapRenderManager()
{
    using This = HdPassthroughRenderManager;
    class_<This, noncopyable>("RenderManager")
        .def(init<>())
        .def("Initialize", &This::Initialize)
        .def("Render", &This::Render)
        .def("Cleanup", &This::Cleanup)

        .def("GetRenderData", &This::GetRenderData,
             return_value_policy<return_by_value>())

        .def("GetSceneDelegateId", &This::GetSceneDelegateId)
        .staticmethod("GetSceneDelegateId")
        ;
}
