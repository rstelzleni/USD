#ifndef USD_IMAGING_HYDRA_PASSTHROUGH_MESH_H
#define USD_IMAGING_HYDRA_PASSTHROUGH_MESH_H

#include "pxr/pxr.h"

#include "pxr/usdImaging/hydraPassthrough/fvarTopologyTracker.h"
#include "pxr/usdImaging/hydraPassthrough/renderData.h"
#include "pxr/imaging/hd/mesh.h"

#include "pxr/base/gf/matrix4f.h"

PXR_NAMESPACE_OPEN_SCOPE

class HydraPassthroughResourceRegistry;

/// \class HydraPassthroughMesh
///
/// This class is an example of a Hydra Rprim, or renderable object, and it
/// gets created on a call to HdRenderIndex::InsertRprim() with a type of
/// HdPrimTypeTokens->mesh.
///
/// The prim object's main function is to bridge the scene description and the
/// renderable representation. The Hydra image generation algorithm will call
/// HdRenderIndex::SyncAll() before any drawing; this, in turn, will call
/// Sync() for each mesh with new data.
///
/// Sync() is passed a set of dirtyBits, indicating which scene buffers are
/// dirty. It uses these to pull all of the new scene data and constructs
/// updated geometry objects.
///
/// An rprim's state is lazily populated in Sync(); matching this, Finalize()
/// can do the heavy work of releasing state (such as handles into the top-level
/// scene), so that object population and existence aren't tied to each other.
///
class HydraPassthroughMesh final : public HdMesh {
public:
    HF_MALLOC_TAG_NEW("new HydraPassthroughMesh");

    /// HydraPassthroughMesh constructor.
    ///   \param id The scene-graph path to this mesh.
    HydraPassthroughMesh(SdfPath const &id);

    /// HydraPassthroughMesh destructor.
    ~HydraPassthroughMesh() override = default;

    /// Inform the scene graph which state needs to be downloaded in the
    /// first Sync() call: in this case, everything, since we don't
    /// re-render this prim after creation.
    ///   \return The initial dirty state this mesh wants to query.
    HdDirtyBits GetInitialDirtyBitsMask() const override;

    /// Pull invalidated scene data and prepare/update the renderable
    /// representation.
    ///
    /// The contract for this function is that the prim can only pull on scene
    /// delegate buffers that are marked dirty. Scene delegates can and do
    /// implement just-in-time data schemes that mean that pulling on clean
    /// data will be at best incorrect, and at worst a crash.
    ///
    /// This function is called in parallel from worker threads, so it needs
    /// to be threadsafe; calls into HdSceneDelegate are ok.
    ///
    /// Reprs are used by hydra for controlling per-item draw settings like
    /// flat/smooth shaded, wireframe, refined, etc.
    ///   \param sceneDelegate The data source for this geometry item.
    ///   \param renderParam State.
    ///   \param dirtyBits A specifier for which scene data has changed.
    ///   \param reprToken A specifier for which representation to draw with.
    ///
    void Sync(HdSceneDelegate *sceneDelegate, HdRenderParam *renderParam,
            HdDirtyBits *dirtyBits, TfToken const &reprToken) override;

    std::string ToString() const;

protected:
    // Initialize the given representation of this Rprim.
    // This is called prior to syncing the prim, the first time the repr
    // is used.
    //
    // reprToken is the name of the repr to initalize.
    //
    // dirtyBits is an in/out value.  It is initialized to the dirty bits
    // from the change tracker.  InitRepr can then set additional dirty bits
    // if additional data is required from the scene delegate when this
    // repr is synced.  InitRepr occurs before dirty bit propagation.
    //
    // See HdRprim::InitRepr()
    void _InitRepr(TfToken const &reprToken, HdDirtyBits *dirtyBits) override;

    // This callback from Rprim gives the prim an opportunity to set
    // additional dirty bits based on those already set.  This is done
    // before the dirty bits are passed to the scene delegate, so can be
    // used to communicate that extra information is needed by the prim to
    // process the changes.
    //
    // This is a pure virtual function so we have to provide, but it is a noop
    HdDirtyBits _PropagateDirtyBits(HdDirtyBits bits) const override;

    // This class does not support copying.
    HydraPassthroughMesh(const HydraPassthroughMesh &) = delete;
    HydraPassthroughMesh &operator=(const HydraPassthroughMesh &) = delete;

private:

    // Once a new topology has been created in Sync, this function updates
    // any dependent computations, like subdivision, quadrangulation,
    // primvars, etc.
    void _UpdateTopologyDependentComputations(
        HdSceneDelegate* sceneDelegate,
        const std::shared_ptr<HydraPassthroughResourceRegistry> &resourceRegistry,
        HdMeshReprDesc const &desc,
        HdReprSharedPtr &repr,
        HdDirtyBits     *dirtyBits);


    // Fills out the mesh data for this mesh. Pulled out from Sync for
    // readability, Sync has a lot of responsibilities.
    void _PopulateMeshValues(HdSceneDelegate* sceneDelegate,
                            HdDirtyBits*     dirtyBits,
                            HdMeshReprDesc const &desc,
                            HdReprSharedPtr &repr);

    HydraPassthroughRenderData::MeshData _meshData;

    // Internal mesh data, not available in python
    HydraPassthroughMeshTopology _topology;
    HydraPassthroughFvarTopologyTracker _fvarTopologyTracker;

    bool _hasMirroredTransform{false};
    bool _sceneNormals{false};
    bool _displayOpacity{false};

    HdInterpolation _sceneNormalsInterpolation{};
};

PXR_NAMESPACE_CLOSE_SCOPE

#endif // USD_IMAGING_HYDRA_PASSTHROUGH_MESH_H
