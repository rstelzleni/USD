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
        .def("initialize", &This::Initialize)
        .def("render", &This::Render)
        .def("cleanup", &This::Cleanup)
        ;
}
