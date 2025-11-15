#include "pxr/pxr.h"
#include "pxr/usdImaging/hydraPassthrough/valueDescriptor.h"
#include "pxr/usd/sdf/assetPath.h"
#include "pxr/usd/sdf/pathExpression.h"
#include "pxr/base/vt/typeHeaders.h"

PXR_NAMESPACE_USING_DIRECTIVE

namespace {
// Type checking utilities. I'd love to use builtin stuff like
// std::remove_all_extents, but VtArray is not a built-in array type,
// so that doesn't work. There don't seem to be equivalent functions
// or typeinfo in those classes, so I'm resorting to the below. I'd
// love to find another approach though.
//
// Note that this approach is not going to work for VtArray<VtArray<T>>
// Only a single dimension deep array is supported, with the built
// in types and their common dimensions.

bool IsFloatingPointType(const VtValue& value) {

#define PROCESS_ENTRY(unused, elem) \
    if (value.IsHolding<VT_TYPE(elem)>()) { return true; } \
    if (value.IsHolding<VtArray<VT_TYPE(elem)>>()) { return true; }
TF_PP_SEQ_FOR_EACH(PROCESS_ENTRY, ~, VT_FLOATING_POINT_BUILTIN_VALUE_TYPES)
TF_PP_SEQ_FOR_EACH(PROCESS_ENTRY, ~, VT_VEC_HALF_VALUE_TYPES)
TF_PP_SEQ_FOR_EACH(PROCESS_ENTRY, ~, VT_VEC_FLOAT_VALUE_TYPES)
TF_PP_SEQ_FOR_EACH(PROCESS_ENTRY, ~, VT_VEC_DOUBLE_VALUE_TYPES)
TF_PP_SEQ_FOR_EACH(PROCESS_ENTRY, ~, VT_MATRIX_FLOAT_VALUE_TYPES)
TF_PP_SEQ_FOR_EACH(PROCESS_ENTRY, ~, VT_MATRIX_DOUBLE_VALUE_TYPES)
TF_PP_SEQ_FOR_EACH(PROCESS_ENTRY, ~, VT_GFRANGE_VALUE_TYPES)
TF_PP_SEQ_FOR_EACH(PROCESS_ENTRY, ~, VT_QUATERNION_VALUE_TYPES)
TF_PP_SEQ_FOR_EACH(PROCESS_ENTRY, ~, VT_DUALQUATERNION_VALUE_TYPES)
#undef PROCESS_ENTRY

    return false;
}

// Note that integral types includes bool, so check that first if you
// need it separate.
bool IsIntegerType(const VtValue& value) {

#define PROCESS_ENTRY(unused, elem) \
    if (value.IsHolding<VT_TYPE(elem)>()) { return true; } \
    if (value.IsHolding<VtArray<VT_TYPE(elem)>>()) { return true; }
TF_PP_SEQ_FOR_EACH(PROCESS_ENTRY, ~, VT_INTEGRAL_BUILTIN_VALUE_TYPES)
TF_PP_SEQ_FOR_EACH(PROCESS_ENTRY, ~, VT_VEC_INT_VALUE_TYPES)
#undef PROCESS_ENTRY
    return false;
}

template <typename T>
std::vector<size_t> _GeValueItemDimension(const VtArray<T>&) {
    // VtArray has a shape data object that's private, and seems to
    // not be populated in usd code. So, we can't rely on that.
    // There seems to be no other api for accessing shape data, so
    // this is being done manually. I'd love to find a better way.
    if constexpr(GfIsGfVec<T>::value) {
        return { T::dimension };
    }
    else if constexpr(GfIsGfMatrix<T>::value) {
        return { T::numRows, T::numColumns };
    }
    else if constexpr(GfIsGfQuat<T>::value) {
        return { 4 };
    }
    else if constexpr(GfIsGfDualQuat<T>::value) {
        return { 8 };
    }
    else if constexpr(GfIsGfRange<T>::value) {
        return { 2 };
    }
    // checks float, int, bool, string, etc. 
#define PROCESS_ENTRY(unused, elem) \
    else if constexpr (std::is_same_v<T, VT_TYPE(elem)>) { \
        return {1}; \
    }
TF_PP_SEQ_FOR_EACH(PROCESS_ENTRY, ~, VT_BUILTIN_VALUE_TYPES)
#undef PROCESS_ENTRY
    return {};
}
 
} // anonymous namespace

HydraPassthroughValueDescriptor::HydraPassthroughValueDescriptor(const VtValue& value)
    : _value(value) 
{}

std::string HydraPassthroughValueDescriptor::ToString() const {
    return TfStringify(_value);
}

VtValue HydraPassthroughValueDescriptor::GetValue() const {
    return _value;
}

TfPyObjWrapper HydraPassthroughValueDescriptor::GetPyValue() const {
    return UsdVtValueToPython(_value);
}

std::string HydraPassthroughValueDescriptor::GetTypeName() const {
    return _value.GetTypeName();
}

HydraPassthroughValueDescriptor::ScalarType
HydraPassthroughValueDescriptor::GetScalarType() const {
    if (IsFloat()) {
        return ScalarType::Float;
    }
    if (IsBool()) {
        // Bool is true for IsInteger as well, so check before checking int
        return ScalarType::Bool;
    }
    if (IsInteger()) {
        return ScalarType::Integer;
    }
    if (IsString()) {
        return ScalarType::String;
    }
    return ScalarType::Unknown;
}

HydraPassthroughValueDescriptor::ElementShape
HydraPassthroughValueDescriptor::GetElementShape() const {
    if (IsMatrix2()) {
        return ElementShape::Matrix2;
    }
    if (IsMatrix3()) {
        return ElementShape::Matrix3;
    }
    if (IsMatrix4()) {
        return ElementShape::Matrix4;
    }
    if (IsQuat()) {
        return ElementShape::Quat;
    }
    if (IsDualQuat()) {
        return ElementShape::DualQuat;
    }
    if (IsVec2()) {
        return ElementShape::Vec2;
    }
    if (IsVec3()) {
        return ElementShape::Vec3;
    }
    if (IsVec4()) {
        return ElementShape::Vec4;
    }
    if (IsRange2()) {
        return ElementShape::Range2;
    }
    if (IsRange3()) {
        return ElementShape::Range3;
    }
    if (IsFloat() || IsInteger() || IsBool() || IsString()) {
        return ElementShape::Scalar;
    }
    return ElementShape::Unknown;
}

bool HydraPassthroughValueDescriptor::IsArray() const {
    return _value.IsArrayValued();
}

bool HydraPassthroughValueDescriptor::IsFloat() const {
    return IsFloatingPointType(_value);
}

bool HydraPassthroughValueDescriptor::IsInteger() const {
    return IsIntegerType(_value);
}

bool HydraPassthroughValueDescriptor::IsBool() const {
    if (_value.IsHolding<bool>()) { return true; }
    if (_value.IsHolding<VtArray<bool>>()) { return true; }
    return false;
}

bool HydraPassthroughValueDescriptor::IsString() const {
    if (_value.IsHolding<std::string>()) { return true; }
    if (_value.IsHolding<TfToken>()) { return true; }
    if (_value.IsHolding<VtArray<std::string>>()) { return true; }
    if (_value.IsHolding<VtArray<TfToken>>()) { return true; }
    return false;
}

bool HydraPassthroughValueDescriptor::IsMatrix2() const {
    if (_value.IsHolding<GfMatrix2f>()) { return true; }
    if (_value.IsHolding<GfMatrix2d>()) { return true; }
    if (_value.IsHolding<VtArray<GfMatrix2f>>()) { return true; }
    if (_value.IsHolding<VtArray<GfMatrix2d>>()) { return true; }
    return false;
}

bool HydraPassthroughValueDescriptor::IsMatrix3() const {
    if (_value.IsHolding<GfMatrix3f>()) { return true; }
    if (_value.IsHolding<GfMatrix3d>()) { return true; }
    if (_value.IsHolding<VtArray<GfMatrix3f>>()) { return true; }
    if (_value.IsHolding<VtArray<GfMatrix3d>>()) { return true; }
    return false;
}

bool HydraPassthroughValueDescriptor::IsMatrix4() const {
    if (_value.IsHolding<GfMatrix4f>()) { return true; }
    if (_value.IsHolding<GfMatrix4d>()) { return true; }
    if (_value.IsHolding<VtArray<GfMatrix4f>>()) { return true; }
    if (_value.IsHolding<VtArray<GfMatrix4d>>()) { return true; }
    return false;
}

bool HydraPassthroughValueDescriptor::IsQuat() const {
    if (_value.IsHolding<GfQuath>()) { return true; }
    if (_value.IsHolding<GfQuatf>()) { return true; }
    if (_value.IsHolding<GfQuatd>()) { return true; }
    if (_value.IsHolding<VtArray<GfQuath>>()) { return true; }
    if (_value.IsHolding<VtArray<GfQuatf>>()) { return true; }
    if (_value.IsHolding<VtArray<GfQuatd>>()) { return true; }
    return false;
}

bool HydraPassthroughValueDescriptor::IsDualQuat() const {
    if (_value.IsHolding<GfDualQuath>()) { return true; }
    if (_value.IsHolding<GfDualQuatf>()) { return true; }
    if (_value.IsHolding<GfDualQuatd>()) { return true; }
    if (_value.IsHolding<VtArray<GfDualQuath>>()) { return true; }
    if (_value.IsHolding<VtArray<GfDualQuatf>>()) { return true; }
    if (_value.IsHolding<VtArray<GfDualQuatd>>()) { return true; }
    return false;
}

bool HydraPassthroughValueDescriptor::IsVec2() const {
    if (_value.IsHolding<GfVec2h>()) { return true; }
    if (_value.IsHolding<GfVec2f>()) { return true; }
    if (_value.IsHolding<GfVec2d>()) { return true; }
    if (_value.IsHolding<GfVec2i>()) { return true; }
    if (_value.IsHolding<VtArray<GfVec2h>>()) { return true; }
    if (_value.IsHolding<VtArray<GfVec2f>>()) { return true; }
    if (_value.IsHolding<VtArray<GfVec2d>>()) { return true; }
    if (_value.IsHolding<VtArray<GfVec2i>>()) { return true; }
    return false;
}

bool HydraPassthroughValueDescriptor::IsVec3() const {
    if (_value.IsHolding<GfVec3h>()) { return true; }
    if (_value.IsHolding<GfVec3f>()) { return true; }
    if (_value.IsHolding<GfVec3d>()) { return true; }
    if (_value.IsHolding<GfVec3i>()) { return true; }
    if (_value.IsHolding<VtArray<GfVec3h>>()) { return true; }
    if (_value.IsHolding<VtArray<GfVec3f>>()) { return true; }
    if (_value.IsHolding<VtArray<GfVec3d>>()) { return true; }
    if (_value.IsHolding<VtArray<GfVec3i>>()) { return true; }
    return false;
}

bool HydraPassthroughValueDescriptor::IsVec4() const {
    if (_value.IsHolding<GfVec4h>()) { return true; }
    if (_value.IsHolding<GfVec4f>()) { return true; }
    if (_value.IsHolding<GfVec4d>()) { return true; }
    if (_value.IsHolding<GfVec4i>()) { return true; }
    if (_value.IsHolding<VtArray<GfVec4h>>()) { return true; }
    if (_value.IsHolding<VtArray<GfVec4f>>()) { return true; }
    if (_value.IsHolding<VtArray<GfVec4d>>()) { return true; }
    if (_value.IsHolding<VtArray<GfVec4i>>()) { return true; }
    return false;
}

bool HydraPassthroughValueDescriptor::IsRange2() const {
    if (_value.IsHolding<GfRange2f>()) { return true; }
    if (_value.IsHolding<GfRange2d>()) { return true; }
    if (_value.IsHolding<VtArray<GfRange2f>>()) { return true; }
    if (_value.IsHolding<VtArray<GfRange2d>>()) { return true; }
    return false;
}

bool HydraPassthroughValueDescriptor::IsRange3() const {
    if (_value.IsHolding<GfRange3f>()) { return true; }
    if (_value.IsHolding<GfRange3d>()) { return true; }
    if (_value.IsHolding<VtArray<GfRange3f>>()) { return true; }
    if (_value.IsHolding<VtArray<GfRange3d>>()) { return true; }
    return false;
}

std::vector<size_t>
HydraPassthroughValueDescriptor::GetArrayItemDimension() const {
    if (IsArray()) {
        // VtValue, yeesh.
#define PROCESS_ENTRY(unused, elem) \
        if (_value.IsHolding<VtArray<VT_TYPE(elem)>>()) { \
            return _GeValueItemDimension(_value.UncheckedGet<VtArray<VT_TYPE(elem)>>()); \
        }
TF_PP_SEQ_FOR_EACH(PROCESS_ENTRY, ~, VT_VALUE_TYPES)
#undef PROCESS_ENTRY
        // SdfAssetPath and SdfPathExpression can get here, but aren't in VT_VALUE_TYPES
        if (_value.IsHolding<VtArray<SdfAssetPath>>() ||
            _value.IsHolding<VtArray<SdfPathExpression>>()) {
            return { 1 };
        }
        // If we get here, it's an array type we don't know about. Warn
        // and get out.
        TF_WARN("Unknown array type %s held in VtValue, cannot get item dimension.",
                _value.GetTypeName().c_str());
    }
    return {};
}

size_t
HydraPassthroughValueDescriptor::GetArraySize() const {
    return _value.GetArraySize();
}
