#include "pxr/pxr.h"
#include "test.h"

#include "pxr/external/boost/python.hpp"
#include "pxr/external/boost/python/class.hpp"
#include "pxr/external/boost/python/def.hpp"

PXR_NAMESPACE_USING_DIRECTIVE

using namespace pxr_boost::python;

int wtf()
{
    return 42;
}

void
wrapTest()
{
    def("wtf", &wtf);

    class_<Test>("Test")
        .def(init<>())
        .def("thingy", &Test::GetThingy)
        ;
}
