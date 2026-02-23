#ifndef PXR_USD_IMAGING_HYDRA_PASSTHROUGH_RENDER_DATA_H
#define PXR_USD_IMAGING_HYDRA_PASSTHROUGH_RENDER_DATA_H

#include "pxr/pxr.h"

#include "pxr/usdImaging/hydraPassthrough/materialParam.h"
#include "pxr/usdImaging/hydraPassthrough/meshTopology.h"
#include "pxr/usdImaging/hydraPassthrough/resourceRegistry.h"
#include "pxr/usdImaging/hydraPassthrough/textureDescriptor.h"
#include "pxr/imaging/hd/enums.h"

#include "pxr/usd/sdf/path.h"

#include "pxr/base/gf/matrix4f.h"
#include "pxr/base/gf/matrix4d.h"
#include "pxr/base/gf/range1f.h"
#include "pxr/base/gf/vec2f.h"
#include "pxr/base/gf/vec3f.h"
#include "pxr/base/tf/declarePtrs.h"
#include "pxr/base/tf/hashmap.h"
#include "pxr/base/vt/array.h"
#include "pxr/base/vt/dictionary.h"
#include "pxr/base/vt/value.h"

#include <mutex>
#include <string>

PXR_NAMESPACE_OPEN_SCOPE

class HdCamera;
class HydraPassthroughFvarTopologyTracker;

TF_DECLARE_REF_PTRS(HydraPassthroughRenderData);

class HydraPassthroughRenderData :
    public TfRefBase,
    public TfWeakBase
{
public:

    // These do not include indices for indexed primvars, because if this is
    // a subdivision surface these have already been baked in. If we need them
    // for non-subdiv meshes, we'll need to add them in.
    //
    // See also HdSt resourceBinder.cpp for data we might need to add here, and
    // to the mesh. There's more computation done there, especially around
    // instancing and type mapping
    struct PrimvarSource {
        PrimvarSource() = default;
        PrimvarSource(const VtValue& d) : data(d) {}
        PrimvarSource(const VtValue& d, HdInterpolation interpolation) 
            : data(d), interpolation(interpolation) {}

        VtValue data;
        HdInterpolation interpolation;

        // Or possibly the hlsl type? We could add this for clients, but there's
        // a question in my mind about types here. For instance, if hdst would have
        // used uint for a primvar, but we provide the output data as int, the client
        // would need to do a conversion. Do we want to suggest that? And if so, is
        // this the right way to communicate it?
        //
        // Also applies to things like float vs double, dmat vs mat, etc.
        // TfToken glslType;
    };

    // For face varying primvars we need to provide their own indices, separate
    // from the mesh's faceVertexIndices. These are sorted into "channels" which
    // can share an index buffer if they have the same topology.
    struct FaceVaryingChannel {
        int channel;
        VtValue indices;
        std::vector<TfToken> primvars;
    };

    class MeshData {
    public:
        SdfPath id;
        SdfPath materialId;
        bool visible{true};
        GfMatrix4f transform;
        GfMatrix4f transformInverse;
        VtValue points;
        VtValue normals;
        VtIntArray faceVertexIndices; // triangles only
        VtIntArray triangleOriginalFaceIndices;
        VtIntArray triangleEdgeIndices;
        TfHashMap<TfToken, PrimvarSource, TfToken::HashFunctor> primvars;
        std::vector<FaceVaryingChannel> faceVaryingChannels;

        // Not wrapped to python, valid only so long as the rprim mesh exists
        //
        // We need this only to populate the face varying primvar index channels
        HydraPassthroughFvarTopologyTracker *fvarTopologyTracker;
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
        VtDictionary materialMetadata;
        // These are copies of the data from HydraPassthroughMaterial, copied
        // because we are not in control of the lifetime of that Sprim.
        std::vector<HydraPassthroughMaterialParam> materialParams;
        std::vector<HydraPassthroughTextureDescriptor> textureDescriptors;
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

    /// The main render contents, as a type that is copyable in c++
    /// and python.
    class RenderData {
    public:
        TfHashMap<SdfPath, MeshData, TfHash> meshes;
        TfHashMap<SdfPath, CameraData, TfHash> cameras;
        TfHashMap<SdfPath, MaterialData, TfHash> materials;

        const MeshData* GetMesh(const SdfPath& id) const;
        size_t GetMeshCount() const;
        const MeshData* GetMeshByIndex(size_t index) const;
        const CameraData* GetCamera(const SdfPath& id) const;
        size_t GetCameraCount() const;
        const CameraData* GetCameraByIndex(size_t index) const;
        const MaterialData* GetMaterial(const SdfPath& id) const;
        size_t GetMaterialCount() const;
        const MaterialData* GetMaterialByIndex(size_t index) const;
    };

    static HydraPassthroughRenderDataRefPtr New() {
        return TfCreateRefPtr(new HydraPassthroughRenderData());
    }

    ~HydraPassthroughRenderData() = default;

    void AddMesh(const SdfPath& id,
                 const MeshData& meshData);

    void AddCamera(const HdCamera* camera);

    void AddMaterial(const SdfPath& id,
                     const MaterialData& materialData);

    /// Copy a potentially computed primvar source value into the render data.
    void CopyPrimvarBufferSource(
            const SdfPath& id,
            HdBufferSourceSharedPtr const &source,
            HydraPassthroughResourceRegistry::PrimvarSourceType sourceType,
            HdInterpolation interpolation = HdInterpolation::HdInterpolationCount);

    /// Copy a list of potentially computed primvar source values into the
    /// render data.
    ///
    /// These all share a single prim and are of a single type, like index,
    /// points, primvar.
    void CopyPrimvarBufferSources(
            const SdfPath& id,
            HdBufferSourceSharedPtrVector const &sources,
            HydraPassthroughResourceRegistry::PrimvarSourceType sourceType,
            HdInterpolation interpolation = HdInterpolation::HdInterpolationCount);

    /// Extract a copy of the contained RenderData.
    ///
    /// This copy does not have mutexes, and is just a snapshot of the
    /// data at a point in time. It is useful for extracting python
    /// copies that will exist and be cachable even after the render
    /// manager has performed its cleanup.
    RenderData ExtractRenderDataCopy() const;

    // Duplicate some api from RenderData. This is for the contained
    // instance of render data, and is protected by the mutexes.
    size_t GetMeshCount() const;
    const MeshData* GetMesh(const SdfPath& id) const;
    const MeshData* GetMeshByIndex(size_t index) const;
    const CameraData* GetCamera(const SdfPath& id) const;
    size_t GetCameraCount() const;
    const CameraData* GetCameraByIndex(size_t index) const;
    const MaterialData* GetMaterial(const SdfPath& id) const;
    size_t GetMaterialCount() const;
    const MaterialData* GetMaterialByIndex(size_t index) const;
    // End duplicate api

private:
    HydraPassthroughRenderData() = default;

    // Mutexes for accessing data containers.
    //
    // Sync is multithreaded, so we need to handle concurrent access to our
    // data structures.
    mutable std::mutex _meshMutex;
    mutable std::mutex _cameraMutex;
    mutable std::mutex _materialMutex;

    // The actual contained data.
    RenderData _renderData;
};

PXR_NAMESPACE_CLOSE_SCOPE

#endif // PXR_USD_IMAGING_HYDRA_PASSTHROUGH_RENDER_DATA_H
