#pragma once

#include "pxr/pxr.h"

#include "pxr/imaging/hd/enums.h"
#include "pxr/imaging/hd/meshTopology.h"

#include "pxr/usd/sdf/path.h"

#include "pxr/base/gf/matrix4f.h"
#include "pxr/base/gf/matrix4d.h"
#include "pxr/base/gf/range1f.h"
#include "pxr/base/gf/vec2f.h"
#include "pxr/base/gf/vec3f.h"
#include "pxr/base/tf/declarePtrs.h"
#include "pxr/base/tf/hashmap.h"
#include "pxr/base/vt/array.h"
#include "pxr/base/vt/value.h"

#include <string>

PXR_NAMESPACE_OPEN_SCOPE

class HdCamera;

TF_DECLARE_REF_PTRS(HydraPassthroughRenderData);

class HydraPassthroughRenderData :
    public TfRefBase,
    public TfWeakBase
{
public:

    struct PrimvarSource {
        VtValue data;
        HdInterpolation interpolation;
    };

    class MeshData {
    public:
        SdfPath id;
        bool visible = true;
        GfMatrix4f transform;
        VtVec3fArray points;
        //VtVec3fArray normals;
        //VtVec2fArray uvs;
        VtVec3iArray faceVertexIndices; // triangles only

        // Additional data for triangulation.
        VtIntArray triangleOriginalFaceIndices;

        // edges encoded like (I believe these are the only values, due to the
        // triangulation approach)
        //  0        show all edges
        //  1        hide edge [2-0]
        //  2        hide edge [0-1]
        //  3        hide edges [0-1] and [2-0]
        VtIntArray triangleEdgeIndices;

        // Not available in python
        HdMeshTopology topology;
        TfHashMap<TfToken, PrimvarSource, TfToken::HashFunctor> primvarSourceMap;
    };

    class MaterialData {
    public:
        SdfPath id;
        enum class MaterialType {
            Unknown,
            PreviewSurface,
            Volume,
            Other
        };
        MaterialType type = MaterialType::Unknown;
        TfToken tag = TfToken();
    };

    class TextureData {
    public:
        std::string filePath;
        enum class TextureType {
            Uv,
            Field,
            Ptex,
            Udim,
            Other
        };
        TextureType type = TextureType::Other;
    };

    class CameraData {
    public:
        SdfPath id;
        GfMatrix4d transform;
        GfMatrix4d projectionMatrix;
        enum class Projection {
            Perspective,
            Orthographic
        };
        Projection projection = Projection::Perspective;
        float horizontalAperture = 0.0f; // in world units
        float verticalAperture = 0.0f;   // in world units
        float horizontalApertureOffset = 0.0f; // in world units
        float verticalApertureOffset = 0.0f;   // in world units
        float focalLength = 0.0f; // in world units
        GfRange1f clippingRange = GfRange1f(0.1f, 1000.0f); // in world units
        std::vector<GfVec4d> clipPlanes;
        float fStop = 0.0f;
        float focusDistance = 0.0f; // in world units
        bool focusOn = false;
        float dofAspect = 1.0f;
        int splitDiopterCount = 0;
        float splitDiopterAngle = 0.0f;
        float splitDiopterOffset1 = 0.0f;
        float splitDiopterWidth1 = 0.0f;
        float splitDiopterFocusDistance1 = 0.0f;
        float splitDiopterOffset2 = 0.0f;
        float splitDiopterWidth2 = 0.0f;
        float splitDiopterFocusDistance2 = 0.0f;
        double shutterOpen = 0.0;
        double shutterClose = 0.0;
        float linearExposureScale = 0.0f;
        TfToken lensDistortionType;
        float lensDistortionK1 = 0.0f;
        float lensDistortionK2 = 0.0f;
        GfVec2f lensDistortionCenter = GfVec2f(0.0f);
        float lensDistortionAnaSq = 1.0f;
        GfVec2f lensDistortionAsym = GfVec2f(0.0f);
        float lensDistortionScale = 1.0f;
        float lensDistortionIor = 0.0f;
        enum class WindowPolicy {
            MatchVertically,
            MatchHorizontally,
            Fit,
            Crop,
            None
        };
        WindowPolicy windowPolicy = WindowPolicy::Fit;
    };

    static HydraPassthroughRenderDataRefPtr New() {
        return TfCreateRefPtr(new HydraPassthroughRenderData());
    }

    ~HydraPassthroughRenderData() = default;

    void SetSceneDelegateId(const SdfPath& id) {
        _sceneDelegateId = id;
    }

    const SdfPath& GetSceneDelegateId() const {
        return _sceneDelegateId;
    }

    void AddMesh(const SdfPath& id,
                 const MeshData& meshData);

    size_t GetMeshCount() const {
        return _meshes.size();
    }

    const MeshData& GetMesh(const SdfPath& id) const;

    const MeshData& GetMeshByIndex(size_t index) const;

    void AddCamera(const HdCamera* camera);

    size_t GetCameraCount() const {
        return _cameras.size();
    }

    const CameraData* GetCameraByIndex(size_t index) const;

    void AddMaterial(const SdfPath& id,
                     const MaterialData& materialData);

    const MaterialData* GetMaterial(const SdfPath& id) const;

    size_t GetMaterialCount() const {
        return _materials.size();
    }

    const MaterialData* GetMaterialByIndex(size_t index) const;

private:
    HydraPassthroughRenderData() = default;

    SdfPath _sceneDelegateId;

    // A map to store mesh data by their SdfPath identifiers.
    TfHashMap<SdfPath, MeshData, TfHash> _meshes;

    TfHashMap<SdfPath, CameraData, TfHash> _cameras;

    TfHashMap<SdfPath, MaterialData, TfHash> _materials;

    // Default mesh data to return in case of errors.
    MeshData _defaultMeshData;
};

PXR_NAMESPACE_CLOSE_SCOPE
