//
// Copyright 2020 Pixar
//
// Licensed under the terms set forth in the LICENSE.txt file available at
// https://openusd.org/license.
//
#include "pxr/usdImaging/usdImaging/dataSourceAttribute.h"

#include "pxr/imaging/hd/dataSourceLocator.h"

#include "pxr/usd/usdShade/udimUtils.h"

PXR_NAMESPACE_OPEN_SCOPE

namespace {

// We need to find the first layer that changes the value
// of the parameter so that we anchor relative paths to that.
static
SdfLayerHandle
_FindLayerHandle(const UsdAttribute& attr, const UsdTimeCode& time) {
    for (const auto& spec: attr.GetPropertyStack(time)) {
        if (spec->HasDefaultValue() ||
            spec->GetLayer()->GetNumTimeSamplesForPath(
                spec->GetPath()) > 0) {
            return spec->GetLayer();
        }
    }
    return TfNullPtr;
}

class UsdImagingDataSourceAssetPathAttribute :
    public UsdImagingDataSourceAttribute<SdfAssetPath>
{
public:
    HD_DECLARE_DATASOURCE(UsdImagingDataSourceAssetPathAttribute);

    /// Returns the extracted SdfAssetPath value of the attribute at
    /// \p shutterOffset, with proper handling for UDIM paths.
    SdfAssetPath
    GetTypedValue(const HdSampledDataSource::Time shutterOffset) override
    {
        using Parent = UsdImagingDataSourceAttribute<SdfAssetPath>;
        SdfAssetPath result = Parent::GetTypedValue(shutterOffset);
        if (!UsdShadeUdimUtils::IsUdimIdentifier(result.GetAssetPath())) {
            return result;
        }
        UsdTimeCode time = _stageGlobals.GetTime();
        if (time.IsNumeric()) {
            time = UsdTimeCode(time.GetValue() + shutterOffset);
        }
        const std::string resolvedPath = UsdShadeUdimUtils::ResolveUdimPath(
            result.GetAssetPath(),
            _FindLayerHandle(_usdAttrQuery.GetAttribute(), time));
        if (!resolvedPath.empty()) {
            result = SdfAssetPath(result.GetAssetPath(), resolvedPath);
        }
        return result;
    }

protected:
    UsdImagingDataSourceAssetPathAttribute(
            const UsdAttribute &usdAttr,
            const UsdImagingDataSourceStageGlobals &stageGlobals,
            const SdfPath &sceneIndexPath = SdfPath::EmptyPath(),
            const HdDataSourceLocator &timeVaryingFlagLocator =
                    HdDataSourceLocator::EmptyLocator())
        : UsdImagingDataSourceAttribute<SdfAssetPath>(
            usdAttr, stageGlobals, sceneIndexPath, timeVaryingFlagLocator)
    { }

    UsdImagingDataSourceAssetPathAttribute(
            const UsdAttributeQuery &usdAttrQuery,
            const UsdImagingDataSourceStageGlobals &stageGlobals,
            const SdfPath &sceneIndexPath = SdfPath::EmptyPath(),
            const HdDataSourceLocator &timeVaryingFlagLocator =
                    HdDataSourceLocator::EmptyLocator())
        : UsdImagingDataSourceAttribute<SdfAssetPath>(
            usdAttrQuery, stageGlobals, sceneIndexPath, timeVaryingFlagLocator)
    { }
};

typedef HdSampledDataSourceHandle (*_DataSourceFactory)(
        const UsdAttributeQuery &usdAttrQuery,
        const UsdImagingDataSourceStageGlobals &stageGlobals,
        const SdfPath &sceneIndexPath,
        const HdDataSourceLocator &timeVaryingFlagLocator);

using _FactoryMap = std::unordered_map<SdfValueTypeName, _DataSourceFactory,
      SdfValueTypeNameHash>;

template <typename T>
HdSampledDataSourceHandle _FactoryImpl(
        const UsdAttributeQuery &usdAttrQuery,
        const UsdImagingDataSourceStageGlobals &stageGlobals,
        const SdfPath &sceneIndexPath,
        const HdDataSourceLocator &timeVaryingFlagLocator)
{
    return UsdImagingDataSourceAttribute<T>::New(
            usdAttrQuery, stageGlobals, sceneIndexPath, timeVaryingFlagLocator);
}

template <>
HdSampledDataSourceHandle _FactoryImpl<SdfAssetPath>(
        const UsdAttributeQuery &usdAttrQuery,
        const UsdImagingDataSourceStageGlobals &stageGlobals,
        const SdfPath &sceneIndexPath,
        const HdDataSourceLocator &timeVaryingFlagLocator)
{
    return UsdImagingDataSourceAssetPathAttribute::New(
            usdAttrQuery, stageGlobals, sceneIndexPath, timeVaryingFlagLocator);
}

static _FactoryMap _CreateFactoryMap()
{
    _FactoryMap map;

    // Note: xref with pxr/usd/sdf/types.h line 513
    // We're missing:
    // - TimeCode, TimeCodeArray
    // - Frame4d, Frame4dArray
    // - Opaque
    // - Group
    // - PathExpressionArray
    map[SdfValueTypeNames->Asset] = _FactoryImpl<SdfAssetPath>;
    map[SdfValueTypeNames->AssetArray] = _FactoryImpl<VtArray<SdfAssetPath>>;
    map[SdfValueTypeNames->Bool] = _FactoryImpl<bool>;
    map[SdfValueTypeNames->BoolArray] = _FactoryImpl<VtArray<bool>>;
    map[SdfValueTypeNames->Color3h] = _FactoryImpl<GfVec3h>;
    map[SdfValueTypeNames->Color3hArray] = _FactoryImpl<VtArray<GfVec3h>>;
    map[SdfValueTypeNames->Color3f] = _FactoryImpl<GfVec3f>;
    map[SdfValueTypeNames->Color3fArray] = _FactoryImpl<VtArray<GfVec3f>>;
    map[SdfValueTypeNames->Color3d] = _FactoryImpl<GfVec3d>;
    map[SdfValueTypeNames->Color3dArray] = _FactoryImpl<VtArray<GfVec3d>>;
    map[SdfValueTypeNames->Color4h] = _FactoryImpl<GfVec4h>;
    map[SdfValueTypeNames->Color4hArray] = _FactoryImpl<VtArray<GfVec4h>>;
    map[SdfValueTypeNames->Color4f] = _FactoryImpl<GfVec4f>;
    map[SdfValueTypeNames->Color4fArray] = _FactoryImpl<VtArray<GfVec4f>>;
    map[SdfValueTypeNames->Color4d] = _FactoryImpl<GfVec4d>;
    map[SdfValueTypeNames->Color4dArray] = _FactoryImpl<VtArray<GfVec4d>>;
    map[SdfValueTypeNames->Double] = _FactoryImpl<double>;
    map[SdfValueTypeNames->DoubleArray] = _FactoryImpl<VtArray<double>>;
    map[SdfValueTypeNames->Double2] = _FactoryImpl<GfVec2d>;
    map[SdfValueTypeNames->Double2Array] = _FactoryImpl<VtArray<GfVec2d>>;
    map[SdfValueTypeNames->Double3] = _FactoryImpl<GfVec3d>;
    map[SdfValueTypeNames->Double3Array] = _FactoryImpl<VtArray<GfVec3d>>;
    map[SdfValueTypeNames->Double4] = _FactoryImpl<GfVec4d>;
    map[SdfValueTypeNames->Double4Array] = _FactoryImpl<VtArray<GfVec4d>>;
    map[SdfValueTypeNames->Half] = _FactoryImpl<GfHalf>;
    map[SdfValueTypeNames->HalfArray] = _FactoryImpl<VtArray<GfHalf>>;
    map[SdfValueTypeNames->Half2] = _FactoryImpl<GfVec2h>;
    map[SdfValueTypeNames->Half2Array] = _FactoryImpl<VtArray<GfVec2h>>;
    map[SdfValueTypeNames->Half3] = _FactoryImpl<GfVec3h>;
    map[SdfValueTypeNames->Half3Array] = _FactoryImpl<VtArray<GfVec3h>>;
    map[SdfValueTypeNames->Half4] = _FactoryImpl<GfVec4h>;
    map[SdfValueTypeNames->Half4Array] = _FactoryImpl<VtArray<GfVec4h>>;
    map[SdfValueTypeNames->Float] = _FactoryImpl<float>;
    map[SdfValueTypeNames->FloatArray] = _FactoryImpl<VtArray<float>>;
    map[SdfValueTypeNames->Float2] = _FactoryImpl<GfVec2f>;
    map[SdfValueTypeNames->Float2Array] = _FactoryImpl<VtArray<GfVec2f>>;
    map[SdfValueTypeNames->Float3] = _FactoryImpl<GfVec3f>;
    map[SdfValueTypeNames->Float3Array] = _FactoryImpl<VtArray<GfVec3f>>;
    map[SdfValueTypeNames->Float4] = _FactoryImpl<GfVec4f>;
    map[SdfValueTypeNames->Float4Array] = _FactoryImpl<VtArray<GfVec4f>>;
    map[SdfValueTypeNames->Int] = _FactoryImpl<int>;
    map[SdfValueTypeNames->IntArray] = _FactoryImpl<VtArray<int>>;
    map[SdfValueTypeNames->Int2] = _FactoryImpl<GfVec2i>;
    map[SdfValueTypeNames->Int2Array] = _FactoryImpl<VtArray<GfVec2i>>;
    map[SdfValueTypeNames->Int3] = _FactoryImpl<GfVec3i>;
    map[SdfValueTypeNames->Int3Array] = _FactoryImpl<VtArray<GfVec3i>>;
    map[SdfValueTypeNames->Int4] = _FactoryImpl<GfVec4i>;
    map[SdfValueTypeNames->Int4Array] = _FactoryImpl<VtArray<GfVec4i>>;
    map[SdfValueTypeNames->Int64] = _FactoryImpl<int64_t>;
    map[SdfValueTypeNames->Int64Array] = _FactoryImpl<VtArray<int64_t>>;
    map[SdfValueTypeNames->Matrix2d] = _FactoryImpl<GfMatrix2d>;
    map[SdfValueTypeNames->Matrix2dArray] = _FactoryImpl<VtArray<GfMatrix2d>>;
    map[SdfValueTypeNames->Matrix3d] = _FactoryImpl<GfMatrix3d>;
    map[SdfValueTypeNames->Matrix3dArray] = _FactoryImpl<VtArray<GfMatrix3d>>;
    map[SdfValueTypeNames->Matrix4d] = _FactoryImpl<GfMatrix4d>;
    map[SdfValueTypeNames->Matrix4dArray] = _FactoryImpl<VtArray<GfMatrix4d>>;
    map[SdfValueTypeNames->Normal3h] = _FactoryImpl<GfVec3h>;
    map[SdfValueTypeNames->Normal3hArray] = _FactoryImpl<VtArray<GfVec3h>>;
    map[SdfValueTypeNames->Normal3f] = _FactoryImpl<GfVec3f>;
    map[SdfValueTypeNames->Normal3fArray] = _FactoryImpl<VtArray<GfVec3f>>;
    map[SdfValueTypeNames->Normal3d] = _FactoryImpl<GfVec3d>;
    map[SdfValueTypeNames->Normal3dArray] = _FactoryImpl<VtArray<GfVec3d>>;
    map[SdfValueTypeNames->PathExpression] = _FactoryImpl<SdfPathExpression>;
    map[SdfValueTypeNames->Point3h] = _FactoryImpl<GfVec3h>;
    map[SdfValueTypeNames->Point3hArray] = _FactoryImpl<VtArray<GfVec3h>>;
    map[SdfValueTypeNames->Point3f] = _FactoryImpl<GfVec3f>;
    map[SdfValueTypeNames->Point3fArray] = _FactoryImpl<VtArray<GfVec3f>>;
    map[SdfValueTypeNames->Point3d] = _FactoryImpl<GfVec3d>;
    map[SdfValueTypeNames->Point3dArray] = _FactoryImpl<VtArray<GfVec3d>>;
    map[SdfValueTypeNames->Quath] = _FactoryImpl<GfQuath>;
    map[SdfValueTypeNames->QuathArray] = _FactoryImpl<VtArray<GfQuath>>;
    map[SdfValueTypeNames->Quatf] = _FactoryImpl<GfQuatf>;
    map[SdfValueTypeNames->QuatfArray] = _FactoryImpl<VtArray<GfQuatf>>;
    map[SdfValueTypeNames->Quatd] = _FactoryImpl<GfQuatd>;
    map[SdfValueTypeNames->QuatdArray] = _FactoryImpl<VtArray<GfQuatd>>;
    map[SdfValueTypeNames->String] = _FactoryImpl<std::string>;
    map[SdfValueTypeNames->StringArray] = _FactoryImpl<VtArray<std::string>>;
    map[SdfValueTypeNames->TexCoord2h] = _FactoryImpl<GfVec2h>;
    map[SdfValueTypeNames->TexCoord2hArray] = _FactoryImpl<VtArray<GfVec2h>>;
    map[SdfValueTypeNames->TexCoord2f] = _FactoryImpl<GfVec2f>;
    map[SdfValueTypeNames->TexCoord2fArray] = _FactoryImpl<VtArray<GfVec2f>>;
    map[SdfValueTypeNames->TexCoord2d] = _FactoryImpl<GfVec2d>;
    map[SdfValueTypeNames->TexCoord2dArray] = _FactoryImpl<VtArray<GfVec2d>>;
    map[SdfValueTypeNames->TexCoord3h] = _FactoryImpl<GfVec3h>;
    map[SdfValueTypeNames->TexCoord3hArray] = _FactoryImpl<VtArray<GfVec3h>>;
    map[SdfValueTypeNames->TexCoord3f] = _FactoryImpl<GfVec3f>;
    map[SdfValueTypeNames->TexCoord3fArray] = _FactoryImpl<VtArray<GfVec3f>>;
    map[SdfValueTypeNames->TexCoord3d] = _FactoryImpl<GfVec3d>;
    map[SdfValueTypeNames->TexCoord3dArray] = _FactoryImpl<VtArray<GfVec3d>>;
    map[SdfValueTypeNames->Token] = _FactoryImpl<TfToken>;
    map[SdfValueTypeNames->TokenArray] = _FactoryImpl<VtArray<TfToken>>;
    map[SdfValueTypeNames->UChar] = _FactoryImpl<unsigned char>;
    map[SdfValueTypeNames->UCharArray] = _FactoryImpl<VtArray<unsigned char>>;
    map[SdfValueTypeNames->UInt] = _FactoryImpl<unsigned int>;
    map[SdfValueTypeNames->UIntArray] = _FactoryImpl<VtArray<unsigned int>>;
    map[SdfValueTypeNames->UInt64] = _FactoryImpl<uint64_t>;
    map[SdfValueTypeNames->UInt64Array] = _FactoryImpl<VtArray<uint64_t>>;
    map[SdfValueTypeNames->Vector3h] = _FactoryImpl<GfVec3h>;
    map[SdfValueTypeNames->Vector3hArray] = _FactoryImpl<VtArray<GfVec3h>>;
    map[SdfValueTypeNames->Vector3f] = _FactoryImpl<GfVec3f>;
    map[SdfValueTypeNames->Vector3fArray] = _FactoryImpl<VtArray<GfVec3f>>;
    map[SdfValueTypeNames->Vector3d] = _FactoryImpl<GfVec3d>;
    map[SdfValueTypeNames->Vector3dArray] = _FactoryImpl<VtArray<GfVec3d>>;

    return map;
}

}

HdSampledDataSourceHandle
UsdImagingDataSourceAttributeNew(
        const UsdAttributeQuery &usdAttrQuery,
        const UsdImagingDataSourceStageGlobals &stageGlobals,
        const SdfPath &sceneIndexPath,
        const HdDataSourceLocator &timeVaryingFlagLocator)
{
    if (!TF_VERIFY(usdAttrQuery.GetAttribute())) {
        return nullptr;
    }

    static const _FactoryMap _factoryMap = _CreateFactoryMap();

    const _FactoryMap::const_iterator i = _factoryMap.find(
            usdAttrQuery.GetAttribute().GetTypeName());
    if (i != _factoryMap.end()) {
        _DataSourceFactory factory = i->second;
        return factory(usdAttrQuery, stageGlobals,
                sceneIndexPath, timeVaryingFlagLocator);
    } else {
        TF_WARN("<%s> Unable to create attribute datasource for type '%s'",
            usdAttrQuery.GetAttribute().GetPath().GetText(),
            usdAttrQuery.GetAttribute().GetTypeName().GetAsToken().GetText());
        return nullptr;
    }
}

HdSampledDataSourceHandle
UsdImagingDataSourceAttributeNew(
        const UsdAttribute &usdAttr,
        const UsdImagingDataSourceStageGlobals &stageGlobals,
        const SdfPath &sceneIndexPath,
        const HdDataSourceLocator &timeVaryingFlagLocator)
{
    return UsdImagingDataSourceAttributeNew(
            UsdAttributeQuery(usdAttr),
            stageGlobals,
            sceneIndexPath,
            timeVaryingFlagLocator);
}

PXR_NAMESPACE_CLOSE_SCOPE
