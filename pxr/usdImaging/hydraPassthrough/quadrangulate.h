#ifndef PXR_USD_IMAGING_HYDRA_PASSTHROUGH_QUADRANGULATE_H
#define PXR_USD_IMAGING_HYDRA_PASSTHROUGH_QUADRANGULATE_H

#include "pxr/pxr.h"

#include "pxr/imaging/hd/bufferSource.h"

#include "pxr/base/vt/array.h"
#include "pxr/base/tf/token.h"
#include "pxr/usd/sdf/path.h"

PXR_NAMESPACE_OPEN_SCOPE

class HydraPassthroughMeshTopology;

using HydraPassthroughQuadInfoBuilderComputationSharedPtr = 
    std::shared_ptr<class HydraPassthroughQuadInfoBuilderComputation>;

/*
  Computation classes for quadrangulation.

  Based heavily on the HdSt version. This version is CPU only, but we could
  add GPU computations following the HdSt example as well. In the HdSt version
  of the CPU code the CPU computation is done then the resulting buffer is
  copied to the GPU. In the diagram below I'm calling this OUTPUT

   *CPU quadrangulation

    (buffersource)
     QuadIndexBuilderComputation  (quad indices)
      |
      +--QuadrangulateComputation (primvar quadrangulation)

     note: QuadrangulateComputation also copies the original primvars.
           no need to transfer the original primvars to GPU separately.

       +--------------------+
   CPU |  original primvars |
       +--------------------+
                |
                v
       +--------------------+-------------------------+
   CPU |  original primvars | quadrangulated primvars |
       +--------------------+-------------------------+
       <---------------------------------------------->
                    filled by computation
                          |
                          v
                        OUTPUT

 */

/// \class HydraPassthroughQuadInfoBuilderComputation
///
/// Quad info computation.
///
class HydraPassthroughQuadInfoBuilderComputation : public HdNullBufferSource {
public:
    HydraPassthroughQuadInfoBuilderComputation(HydraPassthroughMeshTopology *topology, SdfPath const &id);
    bool Resolve() override;

protected:
    bool _CheckValid() const override;

private:
    SdfPath const _id;
    HydraPassthroughMeshTopology *_topology;
};

/// \class HydraPassthroughQuadIndexBuilderComputation
///
/// Quad indices computation CPU.
///

// Index quadrangulation generates a mapping from triangle ID to authored
// face index domain, called primitiveParams. The primitive params are
// stored alongisde topology index buffers, so that the same aggregation
// locators can be used for such an additional buffer as well. This change
// transforms index buffer from int array to int[3] array or int[4] array at
// first. Thanks to the heterogenius non-interleaved buffer aggregation ability
// in hd, we'll get this kind of buffer layout:
//
// ----+-----------+-----------+------
// ... |i0 i1 i2 i3|i4 i5 i6 i7| ...    index buffer (for quads)
// ----+-----------+-----------+------
// ... |     m0    |     m1    | ...    primitive param buffer (coarse face index)
// ----+-----------+-----------+------

class HydraPassthroughQuadIndexBuilderComputation : public HdComputedBufferSource {
public:
    HydraPassthroughQuadIndexBuilderComputation(
        HydraPassthroughMeshTopology *topology,
        HydraPassthroughQuadInfoBuilderComputationSharedPtr const &quadInfoBuilder,
        SdfPath const &id);
    void GetBufferSpecs(HdBufferSpecVector *specs) const override;
    bool Resolve() override;

    // _primitiveParam and _quadsEdgeIndices are available as chained buffers
    bool HasChainedBuffer() const override;
    HdBufferSourceSharedPtrVector GetChainedBuffers() const override;

protected:
    bool _CheckValid() const override;

private:
    SdfPath const _id;
    HydraPassthroughMeshTopology *_topology;
    HydraPassthroughQuadInfoBuilderComputationSharedPtr _quadInfoBuilder;
    HdBufferSourceSharedPtr _primitiveParam;
    HdBufferSourceSharedPtr _quadsEdgeIndices;
};

/// \class HydraPassthroughQuadrangulateComputation
///
/// CPU quadrangulation.
///
class HydraPassthroughQuadrangulateComputation : public HdComputedBufferSource {
public:
    HydraPassthroughQuadrangulateComputation(HydraPassthroughMeshTopology *topology,
                                HdBufferSourceSharedPtr const &source,
                                HdBufferSourceSharedPtr const &quadInfoBuilder,
                                SdfPath const &id);
    void GetBufferSpecs(HdBufferSpecVector *specs) const override;
    bool Resolve() override;
    HdTupleType GetTupleType() const override;

    bool HasPreChainedBuffer() const override;
    HdBufferSourceSharedPtr GetPreChainedBuffer() const override;

protected:
    bool _CheckValid() const override;

private:
    SdfPath const _id;
    HydraPassthroughMeshTopology *_topology;
    HdBufferSourceSharedPtr _source;
    HdBufferSourceSharedPtr _quadInfoBuilder;
};

/// \class HydraPassthroughQuadrangulateFaceVaryingComputation
///
/// CPU face-varying quadrangulation.
///
class HydraPassthroughQuadrangulateFaceVaryingComputation : public HdComputedBufferSource {
public:
    HydraPassthroughQuadrangulateFaceVaryingComputation(HydraPassthroughMeshTopology *topologoy,
                                           HdBufferSourceSharedPtr const &source,
                                           SdfPath const &id);

    void GetBufferSpecs(HdBufferSpecVector *specs) const override;
    bool Resolve() override;

protected:
    bool _CheckValid() const override;

private:
    SdfPath const _id;
    HydraPassthroughMeshTopology *_topology;
    HdBufferSourceSharedPtr _source;
};

PXR_NAMESPACE_CLOSE_SCOPE

#endif  // PXR_USD_IMAGING_HYDRA_PASSTHROUGH_QUADRANGULATE_H
