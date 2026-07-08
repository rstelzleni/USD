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
#include "pxr/base/vt/types.h"
#include "pxr/base/vt/dictionary.h"
#include "pxr/base/vt/value.h"

#include <map>
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

    // See also HdSt resourceBinder.cpp for data we might need to add here, and
    // to the mesh. It's intended to handle mapping data onto the GPU, but
    // there's more computation done there, especially around instancing and
    // GL type mapping
    struct PrimvarData {
        PrimvarData() = default;
        PrimvarData(const VtValue& d) : data(d) {}
        PrimvarData(const VtValue& d, HdInterpolation interpolation) 
            : data(d), interpolation(interpolation) {}

        VtValue data;
        HdInterpolation interpolation;

        // The type that would be used in glsl code.
        //
        // Or possibly the hlsl type? We could add this for clients, but there's
        // a question in my mind about types here. For instance, if hdst would have
        // used uint for a primvar, but we provide the output data as int, the client
        // would need to do a conversion. Do we want to suggest that? And if so, is
        // this the right way to communicate it?
        //
        // Also applies to things like float vs double, dmat vs mat, etc.
        // TfToken glslType;
    };

    // Face Varying primvars may not share the topology of the mesh's
    // faceVertexIndices. 
    //
    // UVs are a common example, they may be discontinuous across UV seams so
    // they need their own indices. 
    //
    // To support this we have this structure for face varying channels. Each
    // channel has its own set of indices, and a list of primvars that use that
    // channel. For instance, if we have st and st2 primvars that both use the
    // same topology, we would have one channel with the indices for the
    // topology, and both st and st2 would be listed as primvars that use that
    // channel.
    //
    // It would look like this:
    //
    //   faceVaryingChannels: [
    //     { channel: 0, indices: [...], primvars: ["st", "st2"] }
    //   ]
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
        GfMatrix4d transform;
        GfMatrix4d transformInverse;
        VtValue points;
        VtValue normals;
        VtIntArray faceVertexIndices; // triangles only
        VtValue primitiveParam;
        VtValue edgeIndices;
        TfHashMap<TfToken, PrimvarData, TfToken::HashFunctor> primvars;
        std::vector<FaceVaryingChannel> faceVaryingChannels;

        // Instancing. If instancerId is non-empty this mesh is an instancing
        // prototype and should be drawn once per entry in instanceTransforms.
        // The full local-to-world for instance i is, in Gf's row-vector
        // convention:
        //
        //   world = transform * instanceTransforms[i]
        //
        // i.e. the mesh's own transform applies first, then the instance
        // transform. Column-vector clients (e.g. threejs) use the transposed
        // matrices multiplied in the reverse order.
        //
        // An instanced mesh whose instances are all hidden (or masked out)
        // has a non-empty instancerId and an empty instanceTransforms, and
        // should not be drawn. Nested instancing arrives here already
        // flattened, ordered outer-major.
        SdfPath instancerId;
        VtMatrix4dArray instanceTransforms;

        // For each entry in instanceTransforms, the instance index at the
        // instancer level closest to the prototype. For a point instancer
        // prototype this is the index into the authored point arrays
        // (masked/invisible instances are omitted without renumbering), so
        // a drawn instance can be mapped back to a specific point for
        // picking or debugging. With nested instancers only the innermost
        // level is identified.
        VtIntArray instanceIndices;

        // Instance-interpolation primvars (e.g. a displayColor authored on
        // a point instancer), with values gathered per drawn instance so
        // each array is parallel to instanceTransforms. The
        // transform-building primvars (hydra:instanceTranslations/
        // Rotations/Scales/Transforms) are excluded because they are
        // already baked into instanceTransforms. When nested instancers
        // author the same primvar, the level closest to the prototype wins.
        TfHashMap<TfToken, PrimvarData, TfToken::HashFunctor> instancePrimvars;

        // Not wrapped to python, valid only so long as the rprim mesh exists
        //
        // We need this only to populate the face varying primvar index channels
        HydraPassthroughFvarTopologyTracker *fvarTopologyTracker;
    };

    /// Data describing a scene graph instancer
    ///
    /// This exists to map drawn instances of native (scene graph) instancing
    /// back to the scene prims that authored them, primarily for picking in
    /// debug renders. Point instancers do not appear here; for those,
    /// MeshData::instanceIndices already identifies the authored point on the
    /// point instancer prim (the mesh's instancerId).
    ///
    /// Clients should check to see if a picked mesh has a
    /// SceneGraphInstancerData object, and if so, use this to map to the
    /// picked source prim.
    class SceneGraphInstancerData {
    public:
        SdfPath id;

        // For each instance index (the values reported in the prototype
        // meshes' instanceIndices), the stage path of the original
        // instanceable prim that was aggregated into that instance. So for
        // drawn instance k of a prototype mesh m:
        //
        //   instanceOriginPaths.at(m.instanceIndices[k])
        //
        // is the scene prim that instance came from.
        std::map<int, SdfPath> instanceOriginPaths;
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
        TfHashMap<SdfPath, SceneGraphInstancerData, TfHash> sceneGraphInstancers;

        const MeshData* GetMesh(const SdfPath& id) const;
        size_t GetMeshCount() const;
        const MeshData* GetMeshByIndex(size_t index) const;
        const CameraData* GetCamera(const SdfPath& id) const;
        size_t GetCameraCount() const;
        const CameraData* GetCameraByIndex(size_t index) const;
        const MaterialData* GetMaterial(const SdfPath& id) const;
        size_t GetMaterialCount() const;
        const MaterialData* GetMaterialByIndex(size_t index) const;
        const SceneGraphInstancerData* GetSceneGraphInstancer(const SdfPath& id) const;
        size_t GetSceneGraphInstancerCount() const;
        const SceneGraphInstancerData* GetSceneGraphInstancerByIndex(size_t index) const;
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

    void AddSceneGraphInstancer(const SdfPath& id,
                                const SceneGraphInstancerData& instancerData);

    /// Copy a potentially computed primvar source value into the render data.
    void CopyPrimvarBufferSource(
            const SdfPath& id,
            HdBufferSourceSharedPtr const &source,
            HydraPassthroughResourceRegistry::PrimvarSourceType sourceType,
            HdInterpolation interpolation = HdInterpolation::HdInterpolationCount);

    /// Extract a copy of the contained RenderData.
    ///
    /// This copy does not have mutexes, and is just a snapshot of the
    /// data at a point in time. It is useful for extracting python
    /// copies that will exist and be cachable even after the render
    /// manager has performed its cleanup.
    RenderData ExtractRenderDataCopy() const;

    /// Extract a copy of the contained RenderData with all mesh primvars
    /// de-indexed into per-corner (face-varying) buffers.
    ///
    /// Renderers that support only a single index buffer per mesh (e.g.
    /// three.js) cannot combine vertex-indexed positions with separately
    /// indexed face-varying primvars like uvs. In this copy every
    /// non-constant primvar of each mesh, along with the points, holds
    /// one value per corner of faceVertexIndices, in corner order.
    /// Expanded primvars report faceVarying interpolation, the
    /// faceVaryingChannels are consumed (cleared), and faceVertexIndices
    /// becomes the identity, so the geometry can be used non-indexed.
    ///
    /// For refined (subdivided) buffers this also drops the unused coarse
    /// values that OpenSubdiv keeps at the start of each buffer.
    RenderData ExtractDeindexedRenderDataCopy() const;

    /// Extract a copy of the contained RenderData with each mesh welded
    /// into a single-index layout.
    ///
    /// This produces the same renderer-facing contract as
    /// ExtractDeindexedRenderDataCopy — one shared index buffer, all
    /// non-constant primvars parallel to points — but with the minimal
    /// vertex count: corners share an output vertex unless one of their
    /// attributes genuinely differs (a uv seam, a uniform value change),
    /// so buffer sizes stay close to plain indexed geometry instead of
    /// tripling. Face-varying data welds by channel indices where they
    /// exist (refined meshes) and by exact bitwise value equality
    /// otherwise; there is no epsilon. Expanded primvars report vertex
    /// interpolation.
    RenderData ExtractWeldedRenderDataCopy() const;

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
    const SceneGraphInstancerData* GetSceneGraphInstancer(const SdfPath& id) const;
    size_t GetSceneGraphInstancerCount() const;
    const SceneGraphInstancerData* GetSceneGraphInstancerByIndex(size_t index) const;
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
    mutable std::mutex _instancerMutex;

    // The actual contained data.
    RenderData _renderData;
};

PXR_NAMESPACE_CLOSE_SCOPE

#endif // PXR_USD_IMAGING_HYDRA_PASSTHROUGH_RENDER_DATA_H
