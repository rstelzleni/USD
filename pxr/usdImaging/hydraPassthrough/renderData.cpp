#include "renderData.h"

#include "pxr/base/tf/diagnosticLite.h"
#include "pxr/imaging/hd/camera.h"
#include "pxr/imaging/hd/tokens.h"
#include "pxr/imaging/hd/vtBufferSource.h"

#include "pxr/base/vt/value.h"

#include "pxr/base/gf/matrix3d.h"
#include "pxr/base/gf/matrix3f.h"
#include "pxr/base/gf/matrix4d.h"
#include "pxr/base/gf/matrix4f.h"
#include "pxr/base/gf/vec2d.h"
#include "pxr/base/gf/vec2f.h"
#include "pxr/base/gf/vec2i.h"
#include "pxr/base/gf/vec2h.h"
#include "pxr/base/gf/vec3d.h"
#include "pxr/base/gf/vec3f.h"
#include "pxr/base/gf/vec3i.h"
#include "pxr/base/gf/vec3h.h"
#include "pxr/base/gf/vec4d.h"
#include "pxr/base/gf/vec4f.h"
#include "pxr/base/gf/vec4i.h"
#include "pxr/base/gf/vec4h.h"
#include "pxr/base/gf/quath.h"
#include "pxr/base/gf/quatf.h"
#include "pxr/base/gf/quatd.h"


PXR_NAMESPACE_OPEN_SCOPE

const HydraPassthroughRenderData::MeshData*
HydraPassthroughRenderData::RenderData::GetMesh(const SdfPath& id) const {
    auto it = meshes.find(id);
    if (it != meshes.end()) {
        return &(it->second);
    }
    return nullptr;
}

size_t
HydraPassthroughRenderData::RenderData::GetMeshCount() const {
    return meshes.size();
}

const HydraPassthroughRenderData::MeshData*
HydraPassthroughRenderData::RenderData::GetMeshByIndex(size_t index) const {
    if (index < meshes.size()) {
        auto it = meshes.begin();
        std::advance(it, index);
        return &(it->second);
    }
    TF_RUNTIME_ERROR(
        "Index %zu out of bounds for HydraPassthroughRenderData with %zu meshes.",
        index, meshes.size());
    return nullptr;
}

const HydraPassthroughRenderData::CameraData*
HydraPassthroughRenderData::RenderData::GetCamera(const SdfPath& id) const {
    auto it = cameras.find(id);
    if (it != cameras.end()) {
        return &(it->second);
    }
    return nullptr;
}

size_t
HydraPassthroughRenderData::RenderData::GetCameraCount() const {
    return cameras.size();
}

const HydraPassthroughRenderData::CameraData* 
HydraPassthroughRenderData::RenderData::GetCameraByIndex(size_t index) const {
    if (index >= cameras.size()) {
        TF_RUNTIME_ERROR("Camera index %zu out of range [0,%zu)",
            index, cameras.size());
        return nullptr;
    }

    auto it = cameras.begin();
    std::advance(it, index);
    return &(it->second);
}

const HydraPassthroughRenderData::MaterialData* 
HydraPassthroughRenderData::RenderData::GetMaterial(const SdfPath& id) const {
    auto it = materials.find(id);
    if (it != materials.end()) {
        return &(it->second);
    }
    return nullptr;
}

size_t
HydraPassthroughRenderData::RenderData::GetMaterialCount() const {
    return materials.size();
}

const HydraPassthroughRenderData::MaterialData* 
HydraPassthroughRenderData::RenderData::GetMaterialByIndex(size_t index) const {
    if (index >= materials.size()) {
        TF_RUNTIME_ERROR("Material index %zu out of range [0,%zu)",
            index, materials.size());
        return nullptr;
    }

    auto it = materials.begin();
    std::advance(it, index);
    return &(it->second);
}

void
HydraPassthroughRenderData::AddMesh(
    const SdfPath& id,
    const MeshData& meshData) {
    const std::lock_guard<std::mutex> lock(_meshMutex);
    _renderData.meshes[id] = meshData;
}

void
HydraPassthroughRenderData::AddCamera(const HdCamera* camera) {
    if (!camera) {
        TF_RUNTIME_ERROR("Null camera pointer in passthrough AddCamera");
        return;
    }

    // XXX RYANS see also HdSceneDelegate::GetCameraParamValue
    CameraData camData;
    camData.id = camera->GetId();
    camData.transform = camera->GetTransform();
    camData.projectionMatrix = camera->ComputeProjectionMatrix();
    camData.projection = camera->GetProjection() == HdCamera::Perspective ?
        CameraData::Projection::Perspective : CameraData::Projection::Orthographic;
    camData.horizontalAperture = camera->GetHorizontalAperture();
    camData.verticalAperture = camera->GetVerticalAperture();
    camData.horizontalApertureOffset = camera->GetHorizontalApertureOffset();
    camData.verticalApertureOffset = camera->GetVerticalApertureOffset();
    camData.focalLength = camera->GetFocalLength();
    camData.clippingRange = camera->GetClippingRange();
    camData.clipPlanes = camera->GetClipPlanes();
    camData.fStop = camera->GetFStop();
    camData.focusDistance = camera->GetFocusDistance();
    camData.focusOn = camera->GetFocusOn();
    camData.dofAspect = camera->GetDofAspect();
    camData.splitDiopterCount = camera->GetSplitDiopterCount();
    camData.splitDiopterAngle = camera->GetSplitDiopterAngle();
    camData.splitDiopterOffset1 = camera->GetSplitDiopterOffset1();
    camData.splitDiopterWidth1 = camera->GetSplitDiopterWidth1();
    camData.splitDiopterFocusDistance1 = camera->GetSplitDiopterFocusDistance1();
    camData.splitDiopterOffset2 = camera->GetSplitDiopterOffset2();
    camData.splitDiopterWidth2 = camera->GetSplitDiopterWidth2();
    camData.splitDiopterFocusDistance2 = camera->GetSplitDiopterFocusDistance2();
    camData.shutterOpen = camera->GetShutterOpen();
    camData.shutterClose = camera->GetShutterClose();
    camData.linearExposureScale = camera->GetLinearExposureScale();
    camData.lensDistortionType = camera->GetLensDistortionType();
    camData.lensDistortionK1 = camera->GetLensDistortionK1();
    camData.lensDistortionK2 = camera->GetLensDistortionK2();
    camData.lensDistortionCenter = camera->GetLensDistortionCenter();
    camData.lensDistortionAnaSq = camera->GetLensDistortionAnaSq();
    camData.lensDistortionAsym = camera->GetLensDistortionAsym();
    camData.lensDistortionScale = camera->GetLensDistortionScale();
    camData.lensDistortionIor = camera->GetLensDistortionIor();
    camData.linearExposureScale = camera->GetLinearExposureScale();

    switch (camera->GetWindowPolicy()) {
        case CameraUtilMatchVertically:
            camData.windowPolicy = CameraData::WindowPolicy::MatchVertically;
            break;
        case CameraUtilMatchHorizontally:
            camData.windowPolicy = CameraData::WindowPolicy::MatchHorizontally;
            break;
        case CameraUtilFit:
            camData.windowPolicy = CameraData::WindowPolicy::Fit;
            break;
        case CameraUtilCrop:
            camData.windowPolicy = CameraData::WindowPolicy::Crop;
            break;
        case CameraUtilDontConform:
            camData.windowPolicy = CameraData::WindowPolicy::None;
            break;
    }

    {
        const std::lock_guard<std::mutex> lock(_cameraMutex);
        _renderData.cameras[camData.id] = camData;
    }
}

void
HydraPassthroughRenderData::AddMaterial(const SdfPath& id, 
                                        const MaterialData& matData) {
    const std::lock_guard<std::mutex> lock(_materialMutex);
    _renderData.materials[id] = matData;
}

// See the similar map in hd/types.cpp, _MakeTupleTypeMap
using HdTypeTypeCasterMap = std::unordered_map<HdType, std::function<VtValue(const void*)>>;
static inline HdTypeTypeCasterMap _MakeHdTypeTypeCasterMap() {
    return HdTypeTypeCasterMap {
        { HdTypeBool, [](const void* data) { return VtValue(*static_cast<const bool*>(data)); } },
        { HdTypeInt8, [](const void* data) { return VtValue(*static_cast<const int8_t*>(data)); } },
        { HdTypeUInt8, [](const void* data) { return VtValue(*static_cast<const uint8_t*>(data)); } },
        { HdTypeInt16, [](const void* data) { return VtValue(*static_cast<const int16_t*>(data)); } },
        { HdTypeUInt16, [](const void* data) { return VtValue(*static_cast<const uint16_t*>(data)); } },
        { HdTypeInt32, [](const void* data) { return VtValue(*static_cast<const int32_t*>(data)); } },
        { HdTypeInt32Vec2, [](const void* data) { return VtValue(*static_cast<const GfVec2i*>(data)); } },
        { HdTypeInt32Vec3, [](const void* data) { return VtValue(*static_cast<const GfVec3i*>(data)); } },
        { HdTypeInt32Vec4, [](const void* data) { return VtValue(*static_cast<const GfVec4i*>(data)); } }, 
        { HdTypeUInt32, [](const void* data) { return VtValue(*static_cast<const uint32_t*>(data)); } },
        // Note that there is no GfVec2ui, etc, so we have to use the signed versions and hope for no 
        // overflow. This is what HdVtBufferSource does as well.
        { HdTypeUInt32Vec2, [](const void* data) { return VtValue(*static_cast<const GfVec2i*>(data)); } },
        { HdTypeUInt32Vec3, [](const void* data) { return VtValue(*static_cast<const GfVec3i*>(data)); } },
        { HdTypeUInt32Vec4, [](const void* data) { return VtValue(*static_cast<const GfVec4i*>(data)); } },
        { HdTypeFloat, [](const void* data) { return VtValue(*static_cast<const float*>(data)); } },
        { HdTypeFloatVec2, [](const void* data) { return VtValue(*static_cast<const GfVec2f*>(data)); } },
        { HdTypeFloatVec3, [](const void* data) { return VtValue(*static_cast<const GfVec3f*>(data)); } },
        { HdTypeFloatVec4, [](const void* data) { return VtValue(*static_cast<const GfVec4f*>(data)); } },
        { HdTypeFloatMat3, [](const void* data) { return VtValue(*static_cast<const GfMatrix3f*>(data)); } },
        { HdTypeFloatMat4, [](const void* data) { return VtValue(*static_cast<const GfMatrix4f*>(data)); } },
        { HdTypeDouble, [](const void* data) { return VtValue(*static_cast<const double*>(data)); } },
        { HdTypeDoubleVec2, [](const void* data) { return VtValue(*static_cast<const GfVec2d*>(data)); } },
        { HdTypeDoubleVec3, [](const void* data) { return VtValue(*static_cast<const GfVec3d*>(data)); } },
        { HdTypeDoubleVec4, [](const void* data) { return VtValue(*static_cast<const GfVec4d*>(data)); } },
        { HdTypeDoubleMat3, [](const void* data) { return VtValue(*static_cast<const GfMatrix3d*>(data)); } },
        { HdTypeDoubleMat4, [](const void* data) { return VtValue(*static_cast<const GfMatrix4d*>(data)); } },
        { HdTypeHalfFloat, [](const void* data) { return VtValue(*static_cast<const GfHalf*>(data)); } },
        { HdTypeHalfFloatVec2, [](const void* data) { return VtValue(*static_cast<const GfVec2h*>(data)); } },
        { HdTypeHalfFloatVec3, [](const void* data) { return VtValue(*static_cast<const GfVec3h*>(data)); } },
        { HdTypeHalfFloatVec4, [](const void* data) { return VtValue(*static_cast<const GfVec4h*>(data)); } },
        // This type should be unreachable in our code
        // { HdTypeInt32_2_10_10_10_REV, [](const void* data) { return VtValue(*static_cast<const HdVec4f_2_10_10_10_REV*>(data)); } },
    };
}

using HdTypeArrayTypeCasterMap = std::unordered_map<HdType, std::function<VtValue(const void*, size_t)>>;
static inline HdTypeArrayTypeCasterMap _MakeHdArrayTypeCasterMap() {
    return HdTypeArrayTypeCasterMap {
        { HdTypeBool, [](const void* data, size_t n) { auto p = static_cast<const bool*>(data); return VtValue(VtArray<bool>(p, p + n)); } },
        { HdTypeInt8, [](const void* data, size_t n) { auto p = static_cast<const int8_t*>(data); return VtValue(VtArray<int8_t>(p, p + n)); } },
        { HdTypeUInt8, [](const void* data, size_t n) { auto p = static_cast<const uint8_t*>(data); return VtValue(VtArray<uint8_t>(p, p + n)); } },
        { HdTypeInt16, [](const void* data, size_t n) { auto p = static_cast<const int16_t*>(data); return VtValue(VtArray<int16_t>(p, p + n)); } },
        { HdTypeUInt16, [](const void* data, size_t n) { auto p = static_cast<const uint16_t*>(data); return VtValue(VtArray<uint16_t>(p, p + n)); } },
        { HdTypeInt32, [](const void* data, size_t n) { auto p = static_cast<const int32_t*>(data); return VtValue(VtArray<int32_t>(p, p + n)); } },
        { HdTypeInt32Vec2, [](const void* data, size_t n) { auto p = static_cast<const GfVec2i*>(data); return VtValue(VtArray<GfVec2i>(p, p + n)); } },
        { HdTypeInt32Vec3, [](const void* data, size_t n) { auto p = static_cast<const GfVec3i*>(data); return VtValue(VtArray<GfVec3i>(p, p + n)); } },
        { HdTypeInt32Vec4, [](const void* data, size_t n) { auto p = static_cast<const GfVec4i*>(data); return VtValue(VtArray<GfVec4i>(p, p + n)); } }, 
        { HdTypeUInt32, [](const void* data, size_t n) { auto p = static_cast<const uint32_t*>(data); return VtValue(VtArray<uint32_t>(p, p + n)); } },
        { HdTypeUInt32Vec2, [](const void* data, size_t n) { auto p = static_cast<const GfVec2i*>(data); return VtValue(VtArray<GfVec2i>(p, p + n)); } },
        { HdTypeUInt32Vec3, [](const void* data, size_t n) { auto p = static_cast<const GfVec3i*>(data); return VtValue(VtArray<GfVec3i>(p, p + n)); } },
        { HdTypeUInt32Vec4, [](const void* data, size_t n) { auto p = static_cast<const GfVec4i*>(data); return VtValue(VtArray<GfVec4i>(p, p + n)); } },
        { HdTypeFloat, [](const void* data, size_t n) { auto p = static_cast<const float*>(data); return VtValue(VtArray<float>(p, p + n)); } },
        { HdTypeFloatVec2, [](const void* data, size_t n) { auto p = static_cast<const GfVec2f*>(data); return VtValue(VtArray<GfVec2f>(p, p + n)); } },
        { HdTypeFloatVec3, [](const void* data, size_t n) { auto p = static_cast<const GfVec3f*>(data); return VtValue(VtArray<GfVec3f>(p, p + n)); } },
        { HdTypeFloatVec4, [](const void* data, size_t n) { auto p = static_cast<const GfVec4f*>(data); return VtValue(VtArray<GfVec4f>(p, p + n)); } },
        { HdTypeFloatMat3, [](const void* data, size_t n) { auto p = static_cast<const GfMatrix3f*>(data); return VtValue(VtArray<GfMatrix3f>(p, p + n)); } },
        { HdTypeFloatMat4, [](const void* data, size_t n) { auto p = static_cast<const GfMatrix4f*>(data); return VtValue(VtArray<GfMatrix4f>(p, p + n)); } },
        { HdTypeDouble, [](const void* data, size_t n) { auto p = static_cast<const double*>(data); return VtValue(VtArray<double>(p, p + n)); } },
        { HdTypeDoubleVec2, [](const void* data, size_t n) { auto p = static_cast<const GfVec2d*>(data); return VtValue(VtArray<GfVec2d>(p, p + n)); } },
        { HdTypeDoubleVec3, [](const void* data, size_t n) { auto p = static_cast<const GfVec3d*>(data); return VtValue(VtArray<GfVec3d>(p, p + n)); } },
        { HdTypeDoubleVec4, [](const void* data, size_t n) { auto p = static_cast<const GfVec4d*>(data); return VtValue(VtArray<GfVec4d>(p, p + n)); } },
        { HdTypeDoubleMat3, [](const void* data, size_t n) { auto p = static_cast<const GfMatrix3d*>(data); return VtValue(VtArray<GfMatrix3d>(p, p + n)); } },
        { HdTypeDoubleMat4, [](const void* data, size_t n) { auto p = static_cast<const GfMatrix4d*>(data); return VtValue(VtArray<GfMatrix4d>(p, p + n)); } },
        { HdTypeHalfFloat, [](const void* data, size_t n) { auto p = static_cast<const GfHalf*>(data); return VtValue(VtArray<GfHalf>(p, p + n)); } },
        { HdTypeHalfFloatVec2, [](const void* data, size_t n) { auto p = static_cast<const GfVec2h*>(data); return VtValue(VtArray<GfVec2h>(p, p + n)); } },
        { HdTypeHalfFloatVec3, [](const void* data, size_t n) { auto p = static_cast<const GfVec3h*>(data); return VtValue(VtArray<GfVec3h>(p, p + n)); } },
        { HdTypeHalfFloatVec4, [](const void* data, size_t n) { auto p = static_cast<const GfVec4h*>(data); return VtValue(VtArray<GfVec4h>(p, p + n)); } },
        // This type should be unreachable in our code
        // { HdTypeInt32_2_10_10_10_REV, [](const void* data, size_t n) { auto p = static_cast<const HdVec4f_2_10_10_10_REV*>(data); return VtValue(VtArray<HdVec4f_2_10_10_10_REV>(p, p + n));
    };
}

static VtValue
_CastRenderDataToCppType(HdBufferSourceSharedPtr const &source) {
    // Use the types in HdTupleType to cast the void * into a type VtValue recognizes,
    // then return the VtValue.
    static const HdTypeTypeCasterMap typeCasterMap = _MakeHdTypeTypeCasterMap();
    static const HdTypeArrayTypeCasterMap arrayTypeCasterMap = _MakeHdArrayTypeCasterMap();
    const auto &tupleType = source->GetTupleType();
    const auto &data = source->GetData();
    const auto &dataSize = source->GetNumElements();
    if (dataSize == 1) {
        auto casterIt = typeCasterMap.find(tupleType.type);
        if (casterIt == typeCasterMap.end()) {
            TF_RUNTIME_ERROR("Unsupported HdType %d in _CastRenderDataToCppType", tupleType.type);
            return VtValue();
        }
        printf(">> Converting single value of HdType %d using caster %ld\n", tupleType.type, tupleType.count);
        return casterIt->second(data);
    } else if (dataSize > 1) {
        // This is a VtArray type
        auto casterIt = arrayTypeCasterMap.find(tupleType.type);
        if (casterIt == arrayTypeCasterMap.end()) {
            TF_RUNTIME_ERROR("Unsupported HdType %d in _CastRenderDataToCppType", tupleType.type);
            return VtValue();
        }
        printf(">> Converting array value of HdType %d using caster %ld\n", tupleType.type, tupleType.count);
        return casterIt->second(data, dataSize);
    }
    return VtValue();
}

void
HydraPassthroughRenderData::CopyPrimvarBufferSource(
        const SdfPath& id,
        HdBufferSourceSharedPtr const &source,
        HydraPassthroughResourceRegistry::PrimvarSourceType sourceType,
        HdInterpolation interpolation)
{
    // Early out if this is not a primvar, but ultimately we should handle points and indices
    // also maybe transform and transform inverse? others?
    if (sourceType != HydraPassthroughResourceRegistry::PrimvarSourceType::Primvar) {
        return;
    }

    const std::lock_guard<std::mutex> lock(_meshMutex);
    auto meshIt = _renderData.meshes.find(id);
    if (meshIt != _renderData.meshes.end()) {

        auto const &vtSource = std::dynamic_pointer_cast<HdVtBufferSource>(source);
        if (!vtSource) {
            // Not a primvar source, for instance it could be an index builder
            //TF_RUNTIME_ERROR("Expected HdVtBufferSource for primvar source, got %s", typeid(*source).name());
            return;
        }
        const TfToken& name = vtSource->GetName();
        if (name == HdTokens->transform || name == HdTokens->transformInverse) {
            // These are special cases, need to deal with them
            return;
        }

        VtValue value(_CastRenderDataToCppType(source)); //source->GetData(), source->GetTupleType()));

        switch (sourceType) {
            case HydraPassthroughResourceRegistry::PrimvarSourceType::Index:
                meshIt->second.primvars[name] = { value, interpolation };
                //meshIt->second.primvars[name] = { value, HdInterpolation::HdInterpolationCount, TfToken("index") };
                break;
            case HydraPassthroughResourceRegistry::PrimvarSourceType::Points:
                meshIt->second.primvars[name] = { value, interpolation };
                //meshIt->second.primvars[name] = { value, HdInterpolation::HdInterpolationCount, TfToken("points") };
                break;
            case HydraPassthroughResourceRegistry::PrimvarSourceType::Primvar:
                printf("RRR copying primvar source %s, interpolation %s\n", name.GetText(), TfEnum::GetDisplayName(interpolation).c_str());
                meshIt->second.primvars[name] = { value, interpolation };
                //meshIt->second.primvars[name] = { value, HdInterpolation::HdInterpolationCount, TfToken("primvar") };
                break;
            case HydraPassthroughResourceRegistry::PrimvarSourceType::Generic:
                meshIt->second.primvars[name] = { value, interpolation };
                //meshIt->second.primvars[name] = { value, HdInterpolation::HdInterpolationCount, TfToken("generic") };
                break;
        }
    } else {
        TF_RUNTIME_ERROR("Mesh with id %s not found when copying primvar buffer source", id.GetText());
    }
}

void
HydraPassthroughRenderData::CopyPrimvarBufferSources(
        const SdfPath& id,
        HdBufferSourceSharedPtrVector const &sources,
        HydraPassthroughResourceRegistry::PrimvarSourceType sourceType,
        HdInterpolation interpolation)
{
    const std::lock_guard<std::mutex> lock(_meshMutex);
    auto meshIt = _renderData.meshes.find(id);
    if (meshIt != _renderData.meshes.end()) {

        TfToken sourceTypeToken;
        switch (sourceType) {
            case HydraPassthroughResourceRegistry::PrimvarSourceType::Index:
                sourceTypeToken = TfToken("index");
                break;
            case HydraPassthroughResourceRegistry::PrimvarSourceType::Points:
                sourceTypeToken = TfToken("points");
                break;
            case HydraPassthroughResourceRegistry::PrimvarSourceType::Primvar:
                sourceTypeToken = TfToken("primvar");
                break;
            case HydraPassthroughResourceRegistry::PrimvarSourceType::Generic:
                sourceTypeToken = TfToken("generic");
                break;
        }

        for (const auto& source : sources) {
            auto const &vtSource = std::dynamic_pointer_cast<HdVtBufferSource>(source);
            if (!vtSource) {
                TF_RUNTIME_ERROR("Expected HdVtBufferSource for primvar source, got %s", typeid(*source).name());
                return;
            }
            // XXX get name exists on the base class I think
            const TfToken& name = vtSource->GetName();
            VtValue value(_CastRenderDataToCppType(source)); //source->GetData(), source->GetTupleType()));

            if (sourceType == HydraPassthroughResourceRegistry::PrimvarSourceType::Primvar) {
                printf("RRR copying primvar source %s, interpolation %s\n", name.GetText(), TfEnum::GetDisplayName(interpolation).c_str());
            }
            meshIt->second.primvars[name] = { value, interpolation };
        }
    } else {
        TF_RUNTIME_ERROR("Mesh with id %s not found when copying primvar buffer sources", id.GetText());
    }
}

HydraPassthroughRenderData::RenderData
HydraPassthroughRenderData::ExtractRenderDataCopy() const {
    const std::lock_guard<std::mutex> lock1(_meshMutex);
    const std::lock_guard<std::mutex> lock2(_cameraMutex);
    const std::lock_guard<std::mutex> lock3(_materialMutex);
    return _renderData;
}

size_t
HydraPassthroughRenderData::GetMeshCount() const
{
    const std::lock_guard<std::mutex> lock(_meshMutex);
    return _renderData.GetMeshCount();
}

const HydraPassthroughRenderData::MeshData*
HydraPassthroughRenderData::GetMesh(const SdfPath& id) const
{
    const std::lock_guard<std::mutex> lock(_meshMutex);
    return _renderData.GetMesh(id);
}

const HydraPassthroughRenderData::MeshData*
HydraPassthroughRenderData::GetMeshByIndex(size_t index) const
{
    const std::lock_guard<std::mutex> lock(_meshMutex);
    return _renderData.GetMeshByIndex(index);
}

const HydraPassthroughRenderData::CameraData*
HydraPassthroughRenderData::GetCamera(const SdfPath& id) const
{
    const std::lock_guard<std::mutex> lock(_cameraMutex);
    return _renderData.GetCamera(id);
}

size_t
HydraPassthroughRenderData::GetCameraCount() const
{
    const std::lock_guard<std::mutex> lock(_cameraMutex);
    return _renderData.GetCameraCount();
}

const HydraPassthroughRenderData::CameraData*
HydraPassthroughRenderData::GetCameraByIndex(size_t index) const
{
    const std::lock_guard<std::mutex> lock(_cameraMutex);
    return _renderData.GetCameraByIndex(index);
}

const HydraPassthroughRenderData::MaterialData*
HydraPassthroughRenderData::GetMaterial(const SdfPath& id) const
{
    const std::lock_guard<std::mutex> lock(_materialMutex);
    return _renderData.GetMaterial(id);
}

size_t
HydraPassthroughRenderData::GetMaterialCount() const
{
    const std::lock_guard<std::mutex> lock(_materialMutex);
    return _renderData.GetMaterialCount();
}

const HydraPassthroughRenderData::MaterialData *
HydraPassthroughRenderData::GetMaterialByIndex(size_t index) const
{
    const std::lock_guard<std::mutex> lock(_materialMutex);
    return _renderData.GetMaterialByIndex(index);
}

PXR_NAMESPACE_CLOSE_SCOPE
