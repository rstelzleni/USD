#ifndef PXR_USD_IMAGING_HYDRA_PASSTHROUGH_MESH_TOPOLOGY_H
#define PXR_USD_IMAGING_HYDRA_PASSTHROUGH_MESH_TOPOLOGY_H

#include "pxr/pxr.h"

#include "pxr/imaging/hd/meshTopology.h"
#include "pxr/imaging/hd/meshUtil.h"
#include "pxr/usdImaging/hydraPassthrough/quadrangulate.h"
#include "pxr/usdImaging/hydraPassthrough/triangulate.h"

#include <memory>

PXR_NAMESPACE_OPEN_SCOPE

class HydraPassthroughSubdivision;

using HydraPassthroughMeshTopologySharedPtr =
    std::shared_ptr<class HydraPassthroughMeshTopology>;

/// \class HydraPassthroughMeshTopology
class HydraPassthroughMeshTopology : public HdMeshTopology {
public:
    enum RefineMode {
        RefineModeUniform = 0,
        RefineModePatches
    };

    enum QuadsMode {
        QuadsTriangulated = 0,
        QuadsUntriangulated
    };

    // Why not HdInterpolation? using that for now
    /// Specifies type of interpolation to use in refinement
    //enum Interpolation {
    //    INTERPOLATE_VERTEX,
    //    INTERPOLATE_VARYING,
    //    INTERPOLATE_FACEVARYING,
    //};

    HydraPassthroughMeshTopology(
        const HdMeshTopology &src,
        int refineLevel,
        RefineMode refineMode,
        QuadsMode quadsMode);

    virtual ~HydraPassthroughMeshTopology();

    // Move semantics only for this object, it contains unique_ptr members
    HydraPassthroughMeshTopology& operator=(HydraPassthroughMeshTopology&&) = default;

    void SanitizeGeomSubsets();

    /// Get subdivision object
    HydraPassthroughSubdivision *GetSubdivision() const {
        return _subdivision.get();
    }

    /// Sets the quadrangulation struct. HdMeshTopology takes an
    /// ownership of quadInfo (caller shouldn't free)
    void SetQuadInfo(HdQuadInfo const *quadInfo);

    /// Returns the quadrangulation struct.
    HdQuadInfo const *GetQuadInfo() const {
        return _quadInfo.get();
    }

    /// Returns the quads mode (triangulated or untriangulated).
    QuadsMode GetQuadsMode() const {
        return _quadsMode;
    }

    /// True if quads should be triangulated
    bool TriangulateQuads() const {
        return _quadsMode == QuadsTriangulated;
    }

    /// Returns true if the subdivision on this mesh produces
    /// triangles (otherwise quads)
    bool RefinesToTriangles() const;

    /// Returns true if the subdivision of this mesh produces bspline patches
    bool RefinesToBSplinePatches() const;

    /// Returns true if the subdivision of this mesh produces box spline
    /// triangle patches
    bool RefinesToBoxSplineTrianglePatches() const;

    /// \name Face-varying Topologies
    /// @{
    /// Returns the face indices of faces not used in any geom subsets.
    std::vector<int> const *GetNonSubsetFaces() const {
        return _nonSubsetFaces.get();
    }

    /// Sets the face-varying topologies.
    void SetFvarTopologies(std::vector<VtIntArray> const &fvarTopologies) {
        _fvarTopologies = fvarTopologies;
    }

    /// Returns the face-varying topologies.
    std::vector<VtIntArray> const & GetFvarTopologies()  {
        return _fvarTopologies;
    }
    /// @}

    // Computations other computations depend on
    // ------------------------------------------------------------
    HdBufferSourceSharedPtr GetOsdTopologyComputation(SdfPath const &debugId);
    HydraPassthroughQuadInfoBuilderComputationSharedPtr
        GetQuadInfoBuilderComputation(SdfPath const &id);
    // ------------------------------------------------------------

    // Client requested computations
    // ------------------------------------------------------------
    HdBufferSourceSharedPtr GetPointsIndexBuilderComputation();
    HdBufferSourceSharedPtr GetTriangleIndexBuilderComputation(SdfPath const &id);
    HdBufferSourceSharedPtr GetQuadIndexBuilderComputation(SdfPath const &id);
    HdBufferSourceSharedPtr GetOsdIndexBuilderComputation();
    HdBufferSourceSharedPtr GetOsdFvarIndexBuilderComputation(int channel);
    // ------------------------------------------------------------

    HdBufferSourceSharedPtr GetQuadrangulateComputation(
        HdBufferSourceSharedPtr const &source, SdfPath const &id);

    HdBufferSourceSharedPtr GetQuadrangulateFaceVaryingComputation(
        HdBufferSourceSharedPtr const &source, SdfPath const &id);

    HdBufferSourceSharedPtr GetTriangulateFaceVaryingComputation(
        HdBufferSourceSharedPtr const &source, SdfPath const &id);

    HdBufferSourceSharedPtr GetOsdRefineComputation(
        HdBufferSourceSharedPtr const &source,
        HdInterpolation interpolation,
        int fvarChannel);

private:
    RefineMode _refineMode;
    QuadsMode _quadsMode;
    int _refineLevel;
    
    // Quad info buffers
    std::unique_ptr<HdQuadInfo const> _quadInfo;

    // Face-varying topologies
    std::vector<VtIntArray> _fvarTopologies;

    HdBufferSourceWeakPtr _osdTopologyBuilder;
    std::unique_ptr<HydraPassthroughSubdivision> _subdivision;
    std::unique_ptr<std::vector<int>> _nonSubsetFaces;

    // Hold a weak ptr to this for later use
    std::weak_ptr<HydraPassthroughQuadInfoBuilderComputation> _quadInfoBuilder;

    // No default construction or copying.
    HydraPassthroughMeshTopology() = delete;
    HydraPassthroughMeshTopology(const HydraPassthroughMeshTopology &) = delete;
    HydraPassthroughMeshTopology &operator =(const HydraPassthroughMeshTopology &) = delete;
};

PXR_NAMESPACE_CLOSE_SCOPE

#endif // PXR_USD_IMAGING_HYDRA_PASSTHROUGH_MESH_TOPOLOGY_H
