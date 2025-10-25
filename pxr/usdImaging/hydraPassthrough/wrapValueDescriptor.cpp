#include "pxr/pxr.h"
#include "valueDescriptor.h"

#include "pxr/external/boost/python.hpp"
#include "pxr/external/boost/python/class.hpp"

PXR_NAMESPACE_USING_DIRECTIVE

using namespace pxr_boost::python;

void
wrapValueDescriptor()
{
    using This = HydraPassthroughValueDescriptor;
    scope s = class_<This >("ValueDescriptor", no_init)
        .def("__str__", &This::ToString)
        .def("GetValue", &This::GetPyValue)
        .def("GetTypeName", &This::GetTypeName)
        .def("IsArray", &This::IsArray)
        .def("IsFloat", &This::IsFloat)
        .def("IsInteger", &This::IsInteger)
        .def("IsBool", &This::IsBool)
        .def("IsString", &This::IsString)
        .def("IsMatrix2", &This::IsMatrix2)
        .def("IsMatrix3", &This::IsMatrix3)
        .def("IsMatrix4", &This::IsMatrix4)
        .def("IsQuat", &This::IsQuat)
        .def("IsDualQuat", &This::IsDualQuat)
        .def("IsVec2", &This::IsVec2)
        .def("IsVec3", &This::IsVec3)
        .def("IsVec4", &This::IsVec4)
        .def("IsRange2", &This::IsRange2)
        .def("IsRange3", &This::IsRange3)
        .def("GetArrayItemDimension", &This::GetArrayItemDimension)
        .def("GetArraySize", &This::GetArraySize)
        ;
}


