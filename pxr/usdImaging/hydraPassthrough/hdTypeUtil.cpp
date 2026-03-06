#include "hdTypeUtil.h"

#include "pxr/imaging/hd/tokens.h"

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

#include "pxr/base/vt/array.h"

PXR_NAMESPACE_OPEN_SCOPE

namespace HydraPassthroughHdTypeUtil
{

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

VtValue CastRenderDataToCppType(HdBufferSourceSharedPtr const &source)
{
    // Use the types in HdTupleType to cast the void * into a type VtValue recognizes,
    // then return the VtValue.
    static const HdTypeTypeCasterMap typeCasterMap = _MakeHdTypeTypeCasterMap();
    static const HdTypeArrayTypeCasterMap arrayTypeCasterMap = _MakeHdArrayTypeCasterMap();

    const auto &tupleType = source->GetTupleType();
    const auto &data = source->GetData();
    const auto &dataSize = source->GetNumElements();
    const auto &itemSize = tupleType.count;
    if (dataSize == 1) {
        auto casterIt = typeCasterMap.find(tupleType.type);
        if (casterIt == typeCasterMap.end()) {
            TF_RUNTIME_ERROR("Unsupported HdType %d in CastRenderDataToCppType", tupleType.type);
            return VtValue();
        }
        return casterIt->second(data);
    } else if (dataSize > 1) {
        // This is a VtArray type
        auto casterIt = arrayTypeCasterMap.find(tupleType.type);
        if (casterIt == arrayTypeCasterMap.end()) {
            TF_RUNTIME_ERROR("Unsupported array HdType %d in CastRenderDataToCppType", tupleType.type);
            return VtValue();
        }
        // HdVtBufferSource converts Vt types into buffers suitable for the GPU, and in doing so
        // has some disagreements with VtValue's representation of arrays. A VtArray of GfVec3i
        // indices might be represented as a void* with type HdTypeInt32Vec3, or it may be represented
        // as a void* with type HdTypeInt32 and an itemSize of 3. We need to handle both cases here.
        // Luckily it appears that in HdVtBufferSource the itemSize is always 1 unless we have 
        // multiples of the tupleType.count. So, A HdTypeInt32Vec3 will have item size 1, unless
        // we should have several of them.
        return casterIt->second(data, dataSize * itemSize);
    }
    return VtValue();
}

} // namespace HydraPassthroughHdTypeUtil

PXR_NAMESPACE_CLOSE_SCOPE
