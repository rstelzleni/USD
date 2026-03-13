#ifndef PXR_USD_IMAGING_HYDRA_PASSTHROUGH_NORMAL_COMPUTATIONS_H
#define PXR_USD_IMAGING_HYDRA_PASSTHROUGH_NORMAL_COMPUTATIONS

#include "pxr/pxr.h"

#include "pxr/imaging/hd/bufferSource.h"
#include "pxr/imaging/hd/flatNormals.h"
#include "pxr/imaging/hd/vertexAdjacency.h"

PXR_NAMESPACE_OPEN_SCOPE

/// \class HydraPassthroughFlatNormalsComputationCPU
///
/// Flat normal computation CPU.
///
class HydraPassthroughFlatNormalsComputationCPU : public HdComputedBufferSource
{
public:
    HydraPassthroughFlatNormalsComputationCPU(
        HdMeshTopology const *topology,
        HdBufferSourceSharedPtr const &points,
        TfToken const &dstName,
        bool packed);

    void GetBufferSpecs(HdBufferSpecVector *specs) const override;

    bool Resolve() override;

    TfToken const &GetName() const override;

protected:
    bool _CheckValid() const override;

private:
    HdMeshTopology const *_topology;
    HdBufferSourceSharedPtr const _points;
    TfToken _dstName;
    bool _packed;
};

/// \class HydraPassthroughSmoothNormalsComputationCPU
///
/// Smooth normal computation CPU.
///
class HydraPassthroughSmoothNormalsComputationCPU : public HdComputedBufferSource
{
public:
    HydraPassthroughSmoothNormalsComputationCPU(
        Hd_VertexAdjacency const *adjacency,
        HdBufferSourceSharedPtr const &points,
        TfToken const &dstName,
        HdBufferSourceSharedPtr const &adjacencyBuilder,
        bool packed);

    void GetBufferSpecs(HdBufferSpecVector *specs) const override;

    bool Resolve() override;

    TfToken const &GetName() const override;

protected:
    bool _CheckValid() const override;

private:
    Hd_VertexAdjacency const *_adjacency;
    HdBufferSourceSharedPtr _points;
    TfToken _dstName;
    HdBufferSourceSharedPtr _adjacencyBuilder;
    bool _packed;
};

PXR_NAMESPACE_CLOSE_SCOPE

#endif // PXR_USD_IMAGING_HYDRA_PASSTHROUGH_NORMAL_COMPUTATIONS_H
