#ifndef USD_IMAGING_HYDRA_PASSTHROUGH_SUBDIVISION_H
#define USD_IMAGING_HYDRA_PASSTHROUGH_SUBDIVISION_H

#include "pxr/pxr.h"

#include "pxr/usdImaging/hydraPassthrough/meshTopology.h"
#include "pxr/imaging/hd/bufferSource.h"
#include "pxr/usd/sdf/path.h"

#include <opensubdiv/far/patchTable.h>
#include <opensubdiv/far/stencilTable.h>

PXR_NAMESPACE_OPEN_SCOPE

class HydraPassthroughMeshTopology;

/// \class HydraPassthroughSubdivision
class HydraPassthroughSubdivision {
public:
    HydraPassthroughSubdivision(int refineLevel);
    ~HydraPassthroughSubdivision();

    /// Returns true if the subdivision for \a scheme generates triangles,
    /// instead of quads.
    static bool RefinesToTriangles(TfToken const &scheme);

    /// Returns true if the subdivision for \a scheme generates bspline patches.
    static bool RefinesToBSplinePatches(TfToken const &scheme);

    /// Returns true if the subdivision for \a scheme generates box spline
    /// triangle patches.
    static bool RefinesToBoxSplineTrianglePatches(TfToken const &scheme);

    int GetRefineLevel() const {
        return _refineLevel;
    }

    OpenSubdiv::Far::PatchTable const *GetPatchTable() const {
        return _patchTable.get();
    }

    OpenSubdiv::Far::StencilTable const *
    GetStencilTable(HdInterpolation interpolation,
                    int fvarChannel) const;

    VtIntArray GetRefinedFvarIndices(int channel) const;

    HdBufferSourceSharedPtr CreateTopologyComputation(
        HydraPassthroughMeshTopology *topology,
        SdfPath const &id);

    HdBufferSourceSharedPtr CreateIndexComputation(
        HydraPassthroughMeshTopology *topology,
        HdBufferSourceSharedPtr const &osdTopology);

    HdBufferSourceSharedPtr CreateFvarIndexComputation(
        HydraPassthroughMeshTopology *topology,
        HdBufferSourceSharedPtr const &osdTopology,
        int channel);

    HdBufferSourceSharedPtr
    CreateRefineComputationCPU(
        HydraPassthroughMeshTopology *topology,
        HdBufferSourceSharedPtr const &source,
        HdBufferSourceSharedPtr const &osdTopology,
        HdInterpolation interpolation,
        int fvarChannel);

    /// Takes ownership of stencil tables and patch table
    void SetRefinementTables(
        std::unique_ptr<OpenSubdiv::Far::StencilTable const> && vertexStencils,
        std::unique_ptr<OpenSubdiv::Far::StencilTable const> && varyingStencils,
        std::vector<std::unique_ptr<
            OpenSubdiv::Far::StencilTable const>> && faceVaryingStencils,
        std::unique_ptr<OpenSubdiv::Far::PatchTable const> && patchTable);


    // Refines the given source buffer into the primvarBuffer using CPU stencils
    void RefineCPU(
            HdBufferSourceSharedPtr const & source,
            std::vector<float> * primvarBuffer,
            HdInterpolation interpolation,
            int fvarChannel);

private:
    int _refineLevel;

    std::unique_ptr<OpenSubdiv::Far::StencilTable const> _vertexStencils;
    std::unique_ptr<OpenSubdiv::Far::StencilTable const> _varyingStencils;
    std::vector<std::unique_ptr<
            OpenSubdiv::Far::StencilTable const>> _faceVaryingStencils;
    std::unique_ptr<OpenSubdiv::Far::PatchTable const> _patchTable;

    // No default construction or copying.
    HydraPassthroughSubdivision() = delete;
    HydraPassthroughSubdivision(const HydraPassthroughSubdivision &) = delete;
    HydraPassthroughSubdivision &operator =(const HydraPassthroughSubdivision &) = delete;
};

PXR_NAMESPACE_CLOSE_SCOPE

#endif // USD_IMAGING_HYDRA_PASSTHROUGH_SUBDIVISION_H
