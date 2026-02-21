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
    
    // Can't nest this in render manager because we need to declare it before the
    // Initialize function
    class_<This::RenderSettings>("RenderSettings")
        .def_readwrite("refineLevel", &This::RenderSettings::refineLevel)
        ;

    class_<This, noncopyable>("RenderManager")
        .def(init<>())
        .def("Initialize", &This::Initialize, arg("settings")=This::RenderSettings())
        .def("Render", &This::Render)
        .def("Cleanup", &This::Cleanup)

        .def("GetRenderData", &This::GetRenderData,
             return_value_policy<return_by_value>())

        .def("GetSceneDelegateId", &This::GetSceneDelegateId)
        .staticmethod("GetSceneDelegateId")
        ;

}
