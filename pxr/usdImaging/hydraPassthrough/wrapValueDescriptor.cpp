#include "pxr/pxr.h"
#include "valueDescriptor.h"

#include "pxr/base/vt/typeHeaders.h"

#include "pxr/external/boost/python.hpp"
#include "pxr/external/boost/python/class.hpp"

PXR_NAMESPACE_USING_DIRECTIVE

using namespace pxr_boost::python;

namespace {

    template <typename T>
    list _CreatePyList(const VtArray<T>& arr, const T defaultValue = T())
    {
        // Preallocate the list size using this goofy boost api, first
        // populate with a defaultValue, then multiply to resize to N 
        // defaultValues.
        list result;
        result.append(defaultValue);
        result *= arr.size();
        for (size_t i = 0; i < arr.size(); ++i) {
            result[i] = arr[i];
        }
        return result;
    }

    object _ConvertPODArrays(const VtValue& value) {
#define PROCESS_ENTRY(unused, elem) \
        if (value.IsHolding<VtArray<VT_TYPE(elem)>>()) { \
            return _CreatePyList(value.UncheckedGet<VtArray<VT_TYPE(elem)>>(), (VT_TYPE(elem))(0)); \
        }
TF_PP_SEQ_FOR_EACH(PROCESS_ENTRY, ~, VT_FLOATING_POINT_BUILTIN_VALUE_TYPES)
TF_PP_SEQ_FOR_EACH(PROCESS_ENTRY, ~, VT_INTEGRAL_BUILTIN_VALUE_TYPES)
#undef PROCESS_ENTRY
        return object(); // Indicate no match
    }

    object _ConvertPOD(const VtValue& value) {
#define PROCESS_ENTRY(unused, elem) \
        if (value.IsHolding<VT_TYPE(elem)>()) { \
            return object(value.UncheckedGet<VT_TYPE(elem)>()); \
        }
TF_PP_SEQ_FOR_EACH(PROCESS_ENTRY, ~, VT_FLOATING_POINT_BUILTIN_VALUE_TYPES)
TF_PP_SEQ_FOR_EACH(PROCESS_ENTRY, ~, VT_INTEGRAL_BUILTIN_VALUE_TYPES)
#undef PROCESS_ENTRY
        return object(); // Indicate no match
    }

    template <typename T>
    object _ConvertVec2(const T& vec) {
        return make_tuple(vec[0], vec[1]);
    }

    object _ConvertVec2s(const VtValue& value) {
        if (value.IsHolding<GfVec2h>()) {
            return _ConvertVec2(value.UncheckedGet<GfVec2h>());
        }
        if (value.IsHolding<GfVec2f>()) {
            return _ConvertVec2(value.UncheckedGet<GfVec2f>());
        }
        if (value.IsHolding<GfVec2d>()) {
            return _ConvertVec2(value.UncheckedGet<GfVec2d>());
        }
        if (value.IsHolding<GfVec2i>()) {
            return _ConvertVec2(value.UncheckedGet<GfVec2i>());
        }
        return object(); // Indicate no match
    }

    template <typename T>
    object _ConvertVec2Array(const VtValue& value) {
        if (value.IsHolding<VtArray<T>>()) {
            const VtArray<T> &arr = value.UncheckedGet<VtArray<T>>();
            list result;
            result.append(tuple());
            result *= arr.size();
            for (size_t i = 0; i < arr.size(); ++i) {
                result[i] = _ConvertVec2(arr[i]);
            }
            return result;
        }
        return object(); // Indicate no match
    }

    object _ConvertVec2Arrays(const VtValue& value) {
        if (object result = _ConvertVec2Array<GfVec2h>(value)) {
            return result;
        }
        if (object result = _ConvertVec2Array<GfVec2f>(value)) {
            return result;
        }
        if (object result = _ConvertVec2Array<GfVec2d>(value)) {
            return result;
        }
        if (object result = _ConvertVec2Array<GfVec2i>(value)) {
            return result;
        }
        return object(); // Indicate no match
    }

    template <typename T>
    object _ConvertVec3(const T& vec) {
        return make_tuple(vec[0], vec[1], vec[2]);
    }

    object _ConvertVec3s(const VtValue& value) {
        if (value.IsHolding<GfVec3h>()) {
            return _ConvertVec3(value.UncheckedGet<GfVec3h>());
        }
        if (value.IsHolding<GfVec3f>()) {
            return _ConvertVec3(value.UncheckedGet<GfVec3f>());
        }
        if (value.IsHolding<GfVec3d>()) {
            return _ConvertVec3(value.UncheckedGet<GfVec3d>());
        }
        if (value.IsHolding<GfVec3i>()) {
            return _ConvertVec3(value.UncheckedGet<GfVec3i>());
        }
        return object(); // Indicate no match
    }

    template <typename T>
    object _ConvertVec3Array(const VtValue& value) {
        if (value.IsHolding<VtArray<T>>()) {
            const VtArray<T> &arr = value.UncheckedGet<VtArray<T>>();
            list result;
            result.append(tuple());
            result *= arr.size();
            for (size_t i = 0; i < arr.size(); ++i) {
                result[i] = _ConvertVec3(arr[i]);
            }
            return result;
        }
        return object(); // Indicate no match
    }

    object _ConvertVec3Arrays(const VtValue& value) {
        if (object result = _ConvertVec3Array<GfVec3h>(value)) {
            return result;
        }
        if (object result = _ConvertVec3Array<GfVec3f>(value)) {
            return result;
        }
        if (object result = _ConvertVec3Array<GfVec3d>(value)) {
            return result;
        }
        if (object result = _ConvertVec3Array<GfVec3i>(value)) {
            return result;
        }
        return object(); // Indicate no match
    }

    template <typename T>
    object _ConvertVec4(const T& vec) {
        return make_tuple(vec[0], vec[1], vec[2], vec[3]);
    }

    object _ConvertVec4s(const VtValue& value) {
        if (value.IsHolding<GfVec4h>()) {
            return _ConvertVec4(value.UncheckedGet<GfVec4h>());
        }
        if (value.IsHolding<GfVec4f>()) {
            return _ConvertVec4(value.UncheckedGet<GfVec4f>());
        }
        if (value.IsHolding<GfVec4d>()) {
            return _ConvertVec4(value.UncheckedGet<GfVec4d>());
        }
        if (value.IsHolding<GfVec4i>()) {
            return _ConvertVec4(value.UncheckedGet<GfVec4i>());
        }
        return object(); // Indicate no match
    }

    template <typename T>
    object _ConvertVec4Array(const VtValue& value) {
        if (value.IsHolding<VtArray<T>>()) {
            const VtArray<T> &arr = value.UncheckedGet<VtArray<T>>();
            list result;
            result.append(tuple());
            result *= arr.size();
            for (size_t i = 0; i < arr.size(); ++i) {
                result[i] = _ConvertVec4(arr[i]);
            }
            return result;
        }
        return object(); // Indicate no match
    }

    object _ConvertVec4Arrays(const VtValue& value) {
        if (object result = _ConvertVec4Array<GfVec4h>(value)) {
            return result;
        }
        if (object result = _ConvertVec4Array<GfVec4f>(value)) {
            return result;
        }
        if (object result = _ConvertVec4Array<GfVec4d>(value)) {
            return result;
        }
        if (object result = _ConvertVec4Array<GfVec4i>(value)) {
            return result;
        }
        return object(); // Indicate no match
    }

    template <typename T>
    object _ConvertMatrix(const T& mat) {
        const size_t numRows = T::numRows;;
        const size_t numCols = T::numColumns;
        list matTuple;
        for (size_t r = 0; r < numRows; ++r) {
            for (size_t c = 0; c < numCols; ++c) {
                matTuple.append(mat[r][c]);
            }
        }
        return tuple(matTuple);
    }

    object _ConvertMatrices(const VtValue& value) {
        if (value.IsHolding<GfMatrix2f>()) {
            return _ConvertMatrix(value.UncheckedGet<GfMatrix2f>());
        }
        if (value.IsHolding<GfMatrix3f>()) {
            return _ConvertMatrix(value.UncheckedGet<GfMatrix3f>());
        }
        if (value.IsHolding<GfMatrix4f>()) {
            return _ConvertMatrix(value.UncheckedGet<GfMatrix4f>());
        }
        if (value.IsHolding<GfMatrix2d>()) {
            return _ConvertMatrix(value.UncheckedGet<GfMatrix2d>());
        }
        if (value.IsHolding<GfMatrix3d>()) {
            return _ConvertMatrix(value.UncheckedGet<GfMatrix3d>());
        }
        if (value.IsHolding<GfMatrix4d>()) {
            return _ConvertMatrix(value.UncheckedGet<GfMatrix4d>());
        }
        return object(); // Indicate no match
    }

    template <typename T>
    object _ConvertMatrixArray(const VtValue& value) {
        if (value.IsHolding<VtArray<T>>()) {
            const VtArray<T> &arr = value.UncheckedGet<VtArray<T>>();
            list result;
            result.append(tuple());
            result *= arr.size();
            for (size_t i = 0; i < arr.size(); ++i) {
                result[i] = _ConvertMatrix(arr[i]);
            }
            return result;
        }
        return object(); // Indicate no match
    }

    object _ConvertMatrixArrays(const VtValue& value) {
        if (object result = _ConvertMatrixArray<GfMatrix2f>(value)) {
            return result;
        }
        if (object result = _ConvertMatrixArray<GfMatrix3f>(value)) {
            return result;
        }
        if (object result = _ConvertMatrixArray<GfMatrix4f>(value)) {
            return result;
        }
        if (object result = _ConvertMatrixArray<GfMatrix2d>(value)) {
            return result;
        }
        if (object result = _ConvertMatrixArray<GfMatrix3d>(value)) {
            return result;
        }
        if (object result = _ConvertMatrixArray<GfMatrix4d>(value)) {
            return result;
        }
        return object(); // Indicate no match
    }

    template <typename T>
    object _ConvertRange2(const T& range) {
        return make_tuple(
            make_tuple(range.GetMin()[0], range.GetMin()[1]),
            make_tuple(range.GetMax()[0], range.GetMax()[1])
        );
    }

    object _ConvertRange2s(const VtValue& value) {
        if (value.IsHolding<GfRange2f>()) {
            return _ConvertRange2(value.UncheckedGet<GfRange2f>());
        }
        if (value.IsHolding<GfRange2d>()) {
            return _ConvertRange2(value.UncheckedGet<GfRange2d>());
        }
        return object(); // Indicate no match
    }

    template <typename T>
    object _ConvertRange2Array(const VtValue& value) {
        if (value.IsHolding<VtArray<T>>()) {
            const VtArray<T> &arr = value.UncheckedGet<VtArray<T>>();
            list result;
            result.append(tuple());
            result *= arr.size();
            for (size_t i = 0; i < arr.size(); ++i) {
                result[i] = _ConvertRange2(arr[i]);
            }
            return result;
        }
        return object(); // Indicate no match
    }

    template <typename T>
    object _ConvertRange3(const T& range) {
        return make_tuple(
            make_tuple(range.GetMin()[0], range.GetMin()[1], range.GetMin()[2]),
            make_tuple(range.GetMax()[0], range.GetMax()[1], range.GetMax()[2])
        );
    }

    object _ConvertRange3s(const VtValue& value) {
        if (value.IsHolding<GfRange3f>()) {
            return _ConvertRange3(value.UncheckedGet<GfRange3f>());
        }
        if (value.IsHolding<GfRange3d>()) {
            return _ConvertRange3(value.UncheckedGet<GfRange3d>());
        }
        return object(); // Indicate no match
    }

    template <typename T>
    object _ConvertRange3Array(const VtValue& value) {
        if (value.IsHolding<VtArray<T>>()) {
            const VtArray<T> &arr = value.UncheckedGet<VtArray<T>>();
            list result;
            result.append(tuple());
            result *= arr.size();
            for (size_t i = 0; i < arr.size(); ++i) {
                result[i] = _ConvertRange3(arr[i]);
            }
            return result;
        }
        return object(); // Indicate no match
    }

    object _ConvertRangeArrays(const VtValue& value) {
        if (object result = _ConvertRange2Array<GfRange2f>(value)) {
            return result;
        }
        if (object result = _ConvertRange2Array<GfRange2d>(value)) {
            return result;
        }
        if (object result = _ConvertRange3Array<GfRange3f>(value)) {
            return result;
        }
        if (object result = _ConvertRange3Array<GfRange3d>(value)) {
            return result;
        }
        return object(); // Indicate no match
    }

    template <typename T>
    object _ConvertQuat(const T& quat) {
        return make_tuple(quat.GetReal(),
                          quat.GetImaginary()[0],
                          quat.GetImaginary()[1],
                          quat.GetImaginary()[2]);
    }

    object _ConvertQuats(const VtValue& value) {
        if (value.IsHolding<GfQuath>()) {
            return _ConvertQuat(value.UncheckedGet<GfQuath>());
        }
        if (value.IsHolding<GfQuatf>()) {
            return _ConvertQuat(value.UncheckedGet<GfQuatf>());
        }
        if (value.IsHolding<GfQuatd>()) {
            return _ConvertQuat(value.UncheckedGet<GfQuatd>());
        }
        return object(); // Indicate no match
    }

    template <typename T>
    object _ConvertQuatArray(const VtValue& value) {
        if (value.IsHolding<VtArray<T>>()) {
            const VtArray<T> &arr = value.UncheckedGet<VtArray<T>>();
            list result;
            result.append(tuple());
            result *= arr.size();
            for (size_t i = 0; i < arr.size(); ++i) {
                result[i] = _ConvertQuat(arr[i]);
            }
            return result;
        }
        return object(); // Indicate no match
    }

    object _ConvertQuatArrays(const VtValue& value) {
        if (object result = _ConvertQuatArray<GfQuath>(value)) {
            return result;
        }
        if (object result = _ConvertQuatArray<GfQuatf>(value)) {
            return result;
        }
        if (object result = _ConvertQuatArray<GfQuatd>(value)) {
            return result;
        }
        return object(); // Indicate no match
    }

    template <typename T>
    object _ConvertDualQuat(const T& dq) {
        return make_tuple(
            make_tuple(
                dq.GetReal().GetReal(),
                dq.GetReal().GetImaginary()[0],
                dq.GetReal().GetImaginary()[1],
                dq.GetReal().GetImaginary()[2]),
            make_tuple(
                dq.GetDual().GetReal(),
                dq.GetDual().GetImaginary()[0],
                dq.GetDual().GetImaginary()[1],
                dq.GetDual().GetImaginary()[2])
        );
    }

    object _ConvertDualQuats(const VtValue& value) {
        if (value.IsHolding<GfDualQuath>()) {
            return _ConvertDualQuat(value.UncheckedGet<GfDualQuath>());
        }
        if (value.IsHolding<GfDualQuatf>()) {
            return _ConvertDualQuat(value.UncheckedGet<GfDualQuatf>());
        }
        if (value.IsHolding<GfDualQuatd>()) {
            return _ConvertDualQuat(value.UncheckedGet<GfDualQuatd>());
        }
        return object(); // Indicate no match
    }

    template <typename T>
    object _ConvertDualQuatArray(const VtValue& value) {
        if (value.IsHolding<VtArray<T>>()) {
            const VtArray<T> &arr = value.UncheckedGet<VtArray<T>>();
            list result;
            result.append(tuple());
            result *= arr.size();
            for (size_t i = 0; i < arr.size(); ++i) {
                result[i] = _ConvertDualQuat(arr[i]);
            }
            return result;
        }
        return object(); // Indicate no match
    }

    object _ConvertDualQuatArrays(const VtValue& value) {
        if (object result = _ConvertDualQuatArray<GfDualQuath>(value)) {
            return result;
        }
        if (object result = _ConvertDualQuatArray<GfDualQuatf>(value)) {
            return result;
        }
        if (object result = _ConvertDualQuatArray<GfDualQuatd>(value)) {
            return result;
        }
        return object(); // Indicate no match
    }

    object _ToPythonJSON(const HydraPassthroughValueDescriptor& desc)
    {
        VtValue value = desc.GetValue();

        // In python we need to call json.dumps() (or an equivalent function)
        // on these value types to convert them into the output format.
        // VtValues are mapped using the python buffer protocol, so they can
        // be used in python without copies for common types. However, these
        // types are not directly serializable to JSON by default. If you attempt
        // it you get errors like "Object of type GfVec3f is not JSON serializable".
        //
        // This could be addressed using a custom serializer in python, but
        // doing that means the value conversion is also python, and in
        // experiments this takes several full seconds with kitchen_set. This
        // C++ vale conversion cuts that down significantly, and when paired
        // with a custom serializer for the ValueDescriptor type we get the
        // fastest version I've found so far.
        //
        // One advantage on the python side is that we're using the orjson
        // library for serialization. This is a rust library, so the
        // serialization is still handled in a memory efficient binary code
        // implementation. The serializer converts and frees the data for each
        // primitive as it goes, so memory should not need to grow to the
        // entire size of the converted binary data before converting. It still
        // needs to store the output JSON string, but there's no way around
        // that. Also, the data still needs to be copied into python for the
        // serializer to access it.
        //
        // This does still mean a copy is made, and I'd love to find a faster
        // route. The nuclear option would be to stringify to JSON directly in
        // C++ and provide access to that buffer in python. This still involves
        // a copy into the server, but would avoid a round trip though python
        // for all the binary data. It does not map perfectly to our existing
        // python server library design, so I think this may require patching
        // the server library to support it.
        //

        if (desc.IsArray()) {
            if (const object &vec2Array = _ConvertVec2Arrays(value)) {
                return vec2Array;
            }
            if (const object &vec3Array = _ConvertVec3Arrays(value)) {
                return vec3Array;
            }
            if (const object &vec4Array = _ConvertVec4Arrays(value)) {
                return vec4Array;
            }
            if (const object &matrixArray = _ConvertMatrixArrays(value)) {
                return matrixArray;
            }
            if (const object &rangeArray = _ConvertRangeArrays(value)) {
                return rangeArray;
            }
            if (const object &quatArray = _ConvertQuatArrays(value)) {
                return quatArray;
            }
            if (const object &dualQuatArray = _ConvertDualQuatArrays(value)) {
                return dualQuatArray;
            }
            if (const object &podArray = _ConvertPODArrays(value)) {
                return podArray;
            }
            if (value.IsHolding<VtArray<std::string>>()) {
                return _CreatePyList(value.UncheckedGet<VtArray<std::string>>());
            }
            if (value.IsHolding<VtArray<TfToken>>()) {
                VtArray<TfToken> arr = value.UncheckedGet<VtArray<TfToken>>();
                list result;
                result.append(std::string());
                result *= arr.size();
                for (size_t i = 0; i < arr.size(); ++i) {
                    result[i] = arr[i].GetString();
                }
                return result;
            }
        }
        // Handle single value types
        else {
            if (const object &vec2 = _ConvertVec2s(value)) {
                return vec2;
            }
            if (const object &vec3 = _ConvertVec3s(value)) {
                return vec3;
            }
            if (const object &vec4 = _ConvertVec4s(value)) {
                return vec4;
            }
            if (const object &matrix = _ConvertMatrices(value)) {
                return matrix;
            }
            if (const object &range2 = _ConvertRange2s(value)) {
                return range2;
            }
            if (const object &range3 = _ConvertRange3s(value)) {
                return range3;
            }
            if (const object &quat = _ConvertQuats(value)) {
                return quat;
            }
            if (const object &dualQuat = _ConvertDualQuats(value)) {
                return dualQuat;
            }
            if (const object &pod = _ConvertPOD(value)) {
                return pod;
            }
            if (value.IsHolding<std::string>()) {
                return object(value.UncheckedGet<std::string>());
            }
            if (value.IsHolding<TfToken>()) {
                TfToken token = value.UncheckedGet<TfToken>();
                return object(token.GetString());
            }
        }

        // Fallback for unknown types - convert to string
        return object(TfStringify(value));
    }

} // end anonymous namespace

void
wrapValueDescriptor()
{
    using This = HydraPassthroughValueDescriptor;
    scope s = class_<This >("ValueDescriptor", no_init)
        .def("__str__", &This::ToString)
        .def("GetValue", &This::GetPyValue)
        .def("ToPythonJSON", &_ToPythonJSON)
        .def("GetTypeName", &This::GetTypeName)
        .def("GetScalarType", &This::GetScalarType)
        .def("GetElementShape", &This::GetElementShape)
        .def("IsArray", &This::IsArray)
        .def("GetArrayItemDimension", &This::GetArrayItemDimension)
        .def("GetArraySize", &This::GetArraySize)
         ;

    enum_<This::ScalarType>("ScalarType")
        .value("Unknown", This::ScalarType::Unknown)
        .value("Float", This::ScalarType::Float)
        .value("Integer", This::ScalarType::Integer)
        .value("Bool", This::ScalarType::Bool)
        .value("String", This::ScalarType::String)
        ;

    enum_<This::ElementShape>("ElementShape")
        .value("Unknown", This::ElementShape::Unknown)
        .value("Scalar", This::ElementShape::Scalar)
        .value("Matrix2", This::ElementShape::Matrix2)
        .value("Matrix3", This::ElementShape::Matrix3)
        .value("Matrix4", This::ElementShape::Matrix4)
        .value("Quat", This::ElementShape::Quat)
        .value("DualQuat", This::ElementShape::DualQuat)
        .value("Vec2", This::ElementShape::Vec2)
        .value("Vec3", This::ElementShape::Vec3)
        .value("Vec4", This::ElementShape::Vec4)
        .value("Range2", This::ElementShape::Range2)
        .value("Range3", This::ElementShape::Range3)
        ;
}


