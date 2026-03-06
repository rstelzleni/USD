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
///
/// Manages the subdivision tables from OpenSubdiv and creates computations
/// that use that data.
class HydraPassthroughSubdivision {
public:
    // Hard coded buffer source parameter names.
    //
    // Not tokens because we'll do string operations on them
    static constexpr const char * PrimvarChannelIndexBaseName = "fvarIndices";
    static constexpr const char * FvarPatchParamBaseName = "fvarPatchParam";

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

    /// Given the name of a face-varying primvar channel index, returns the
    /// channel number. For instance, if the name is "fvarIndices3", this
    /// function will return 3. If the name doesn't match the expected pattern,
    /// returns -1.
    static int GetChannelFromPrimvarChannelIndexName(const TfToken &name);

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


    /// Refines the given source buffer into the primvarBuffer using CPU stencils.
    ///
    /// Used by the computations returned from here. The computations expect that
    /// the lifetime of this object will outlive them, because they need this
    /// function and the stencils and patch table that are managed by this object.
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
