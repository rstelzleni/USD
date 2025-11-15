#ifndef USD_IMAGING_HYDRA_PASSTHROUGH_VALUE_DESCRIPTOR_H
#define USD_IMAGING_HYDRA_PASSTHROUGH_VALUE_DESCRIPTOR_H

#include "pxr/pxr.h"

#include "pxr/usd/usd/pyConversions.h"
#include "pxr/base/vt/value.h"

#include <string>
#include <vector>

PXR_NAMESPACE_OPEN_SCOPE

/// An adapter for returning VtValues for rendering values.
///
/// Additional type information is needed by the rendering apis, so 
/// just using UsdVtValueToPython is not enough information.
///
/// This type itself is kind of a useless shim from C++. In python
/// it provides additional apis for vt values.
class HydraPassthroughValueDescriptor {
public:
    HydraPassthroughValueDescriptor() = default;
    HydraPassthroughValueDescriptor(const VtValue& value);

    /// Scalar type of the held value.
    enum class ScalarType {
        Unknown,
        Float,
        Integer,
        Bool,
        String
    };

    /// Element shape of the held value.
    ///
    /// This plus ScalarType gives the type of object being represented,
    /// plus its scalar representation. Then whether it is an array of these
    /// objects is determined separately.
    enum class ElementShape {
        Unknown,
        Scalar,
        Matrix2,
        Matrix3,
        Matrix4,
        Quat,
        DualQuat,
        Vec2,
        Vec3,
        Vec4,
        Range2,
        Range3
    };

    /// Return the held value
    VtValue GetValue() const;

    std::string ToString() const;

    /// Convert VtValues to something we can access in python.
    ///
    /// Quick note about UsdVtValueToPython and the returned TfPyObjWrapper.
    /// In usd/pyConversions.h there is a comment that this conversion function
    /// is deprecated, but gives no alternative to call. It is used all over the
    /// usd lib itself, including in wrapAttribute.cpp, and the deprecated
    /// comment is from before 2016 (i.e. before open sourcing).
    TfPyObjWrapper GetPyValue() const;

    std::string GetTypeName() const;

    ScalarType GetScalarType() const;

    ElementShape GetElementShape() const;

    // These type queries are not fully expressive of what vt value
    // can represent. They're intended to help with creating a python
    // representation of the desired type, and python also is not as
    // expressive with types as c++, so basically, these lose information.
    bool IsArray() const;
    bool IsFloat() const;
    bool IsInteger() const;
    bool IsBool() const; // Bool will also return true for IsInteger
    bool IsString() const;
    bool IsMatrix2() const;
    bool IsMatrix3() const;
    bool IsMatrix4() const;
    bool IsQuat() const;
    bool IsDualQuat() const;
    bool IsVec2() const;
    bool IsVec3() const;
    bool IsVec4() const;
    bool IsRange2() const;
    bool IsRange3() const;

    /// Get the dimensions of the value type held in this array, if it
    /// is an array. Examples (imagine each wrapped in a VtValue):
    /// VtArray<int> -> {1}
    /// VtArray<GfVec3f> -> {3}
    /// VtArray<GfMatrix4d> -> {4, 4}
    /// float -> {}
    /// Returns empty vector if not an array. note that this does not include
    /// array size, only dimensions of the contained type.
    std::vector<size_t> GetArrayItemDimension() const;

    /// If this is an array, return the number of elements in the array.
    /// Otherwise return 0.
    size_t GetArraySize() const;

private:
    VtValue _value;
};

PXR_NAMESPACE_CLOSE_SCOPE

#endif // USD_IMAGING_HYDRA_PASSTHROUGH_VALUE_DESCRIPTOR_H
