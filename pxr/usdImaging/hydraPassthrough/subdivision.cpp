#include "subdivision.h"

#include "pxr/usdImaging/hydraPassthrough/meshTopology.h"

#include "pxr/imaging/hd/bufferArrayRange.h"
#include "pxr/imaging/hd/meshUtil.h"
#include "pxr/imaging/hd/perfLog.h"
#include "pxr/imaging/hd/tokens.h"
#include "pxr/imaging/hd/vtBufferSource.h"
#include "pxr/imaging/hf/perfLog.h"
#include "pxr/imaging/pxOsd/refinerFactory.h"
#include "pxr/imaging/pxOsd/tokens.h"

#include "pxr/base/gf/vec2i.h"
#include "pxr/base/gf/vec3i.h"
#include "pxr/base/gf/vec4i.h"
#include "pxr/base/tf/staticTokens.h"
#include "pxr/base/tf/stringUtils.h"

#include <opensubdiv/version.h>
#include <opensubdiv/far/patchTable.h>
#include <opensubdiv/far/patchTableFactory.h>
#include <opensubdiv/far/stencilTable.h>
#include <opensubdiv/far/stencilTableFactory.h>

PXR_NAMESPACE_OPEN_SCOPE

namespace {

// ---------------------------------------------------------------------------
/// \class Hd_OsdTopologyComputation
///
/// Generates the OpenSubdiv stencil and patch tables and stores them on the
/// owning subdivision object.
///
/// This buffer source doesn't return anything from its GetData() method. I'm
/// not sure why it isn't a HdNullBufferSource, that may be related to
/// scheduling dependencies that exist in HdSt but not here.
class _OsdTopologyComputation final : public HdComputedBufferSource
{
public:
    _OsdTopologyComputation(
            HydraPassthroughMeshTopology *topology,
            SdfPath const &id);

    bool Resolve() override;
    void GetBufferSpecs(HdBufferSpecVector *specs) const override;

protected:
    bool _CheckValid() const override;

private:
    HydraPassthroughMeshTopology *_topology;
    SdfPath const _id;
};

_OsdTopologyComputation::_OsdTopologyComputation(
    HydraPassthroughMeshTopology *topology,
    SdfPath const &id)
    : _topology(topology)
    , _id(id)
{
}

void
_OsdTopologyComputation::GetBufferSpecs(HdBufferSpecVector *specs) const
{
}

bool
_OsdTopologyComputation::Resolve()
{
    using namespace OpenSubdiv;

    if (!_TryLock()) return false;

    HD_TRACE_FUNCTION();
    HF_MALLOC_TAG_FUNCTION();

    // do far analysis and set stencils and patch table

    if (!TF_VERIFY(_topology)) {
        _SetResolved();
        return true;
    }

    auto * subdivision = _topology->GetSubdivision();
    if (!TF_VERIFY(subdivision)) {
        _SetResolved();
        return true;
    }

    // create topology refiner
    PxOsdTopologyRefinerSharedPtr refiner;

    // for empty topology, we don't need to refine anything.
    // but still need to return the typed buffer for codegen
    int numFvarChannels = 0;
    if (_topology->GetFaceVertexCounts().size() == 0) {
        // leave refiner empty
    } else {
        refiner = PxOsdRefinerFactory::Create(_topology->GetPxOsdMeshTopology(),
                                              _topology->GetFvarTopologies(),
                                              TfToken(_id.GetText()));
        numFvarChannels = refiner->GetNumFVarChannels();
    }

    std::unique_ptr<Far::StencilTable const> vertexStencils;
    std::unique_ptr<Far::StencilTable const> varyingStencils;
    std::vector<std::unique_ptr<Far::StencilTable const>>
        faceVaryingStencils(numFvarChannels);
    std::unique_ptr<Far::PatchTable const> patchTable;

    // refine topology and create stencil tables and patch table
    if (refiner) {
        bool const adaptive = false; //subdivision->IsAdaptive();
        int const level = subdivision->GetRefineLevel();

        Far::PatchTableFactory::Options patchOptions(level);
        if (numFvarChannels > 0) {
            patchOptions.generateFVarTables = true;
            patchOptions.includeFVarBaseLevelIndices = true;
            patchOptions.generateFVarLegacyLinearPatches = !adaptive;
        }
        if (adaptive) {
            patchOptions.endCapType =
                Far::PatchTableFactory::Options::ENDCAP_BSPLINE_BASIS;
#if OPENSUBDIV_VERSION_NUMBER >= 30400
            // Improve fidelity when refining to limit surface patches
            // These options supported since v3.1.0 and v3.2.0 respectively.
            patchOptions.useInfSharpPatch = true;
            patchOptions.generateLegacySharpCornerPatches = false;
#endif
        }

        // split trace scopes.
        {
            HD_TRACE_SCOPE("refine");
            if (adaptive) {
                Far::TopologyRefiner::AdaptiveOptions adaptiveOptions(level);
#if OPENSUBDIV_VERSION_NUMBER >= 30400
                adaptiveOptions =  patchOptions.GetRefineAdaptiveOptions();
#endif
                refiner->RefineAdaptive(adaptiveOptions);
            } else {
                refiner->RefineUniform(level);
            }
        }
        {
            HD_TRACE_SCOPE("stencil factory");
            Far::StencilTableFactory::Options options;
            options.generateOffsets = true;
            options.generateIntermediateLevels = adaptive;
            options.interpolationMode =
                Far::StencilTableFactory::INTERPOLATE_VERTEX;
            vertexStencils.reset(
                Far::StencilTableFactory::Create(*refiner, options));

            options.interpolationMode =
                Far::StencilTableFactory::INTERPOLATE_VARYING;
            varyingStencils.reset(
                Far::StencilTableFactory::Create(*refiner, options));

            options.interpolationMode =
                Far::StencilTableFactory::INTERPOLATE_FACE_VARYING;
            for (int i = 0; i < numFvarChannels; ++i) {
                options.fvarChannel = i;
                faceVaryingStencils[i].reset(
                    Far::StencilTableFactory::Create(*refiner, options));
            }
        }
        {
            HD_TRACE_SCOPE("patch factory");
            patchTable.reset(
                Far::PatchTableFactory::Create(*refiner, patchOptions));
        }
    }

    // merge local point stencils
    if (patchTable && patchTable->GetLocalPointStencilTable()) {
        // append stencils
        if (Far::StencilTable const *vertexStencilsWithLocalPoints =
            Far::StencilTableFactory::AppendLocalPointStencilTable(
                *refiner,
                vertexStencils.get(),
                patchTable->GetLocalPointStencilTable())) {
            vertexStencils.reset(vertexStencilsWithLocalPoints);
        }
    }
    if (patchTable && patchTable->GetLocalPointVaryingStencilTable()) {
        // append stencils
        if (Far::StencilTable const *varyingStencilsWithLocalPoints =
            Far::StencilTableFactory::AppendLocalPointStencilTableVarying(
                *refiner,
                varyingStencils.get(),
                patchTable->GetLocalPointVaryingStencilTable())) {
            varyingStencils.reset(varyingStencilsWithLocalPoints);
        }
    }
    for (int i = 0; i < numFvarChannels; ++i) {
        if (patchTable && patchTable->GetLocalPointFaceVaryingStencilTable(i)) {
            // append stencils
            if (Far::StencilTable const *faceVaryingStencilsWithLocalPoints =
                Far::StencilTableFactory
                        ::AppendLocalPointStencilTableFaceVarying(
                    *refiner,
                    faceVaryingStencils[i].get(),
                    patchTable->GetLocalPointFaceVaryingStencilTable(i),
                    i)) {
                faceVaryingStencils[i].reset(
                        faceVaryingStencilsWithLocalPoints);
            }
        }
    }

    // set tables to topology
    subdivision->SetRefinementTables(std::move(vertexStencils),
                                     std::move(varyingStencils),
                                     std::move(faceVaryingStencils),
                                     std::move(patchTable));

    _SetResolved();
    return true;
}

bool
_OsdTopologyComputation::_CheckValid() const
{
    return true;
}


// ---------------------------------------------------------------------------
/// \class _OsdIndexComputation
///
/// OpenSubdiv refined index buffer computation. Generates the refined indices
/// for either quads or triangles depending on the subdivision scheme, and
/// also generates a promitive param buffer that maps refined primitives to
/// the coarse faces they came from, and also stores patch params (e.g. u/v
/// param for limit patches).
///
/// primitiveParam : refined quads to coarse faces mapping buffer
///
/// ----+-----------+-----------+------
/// ... |i0 i1 i2 i3|i4 i5 i6 i7| ...    index buffer (for quads)
/// ----+-----------+-----------+------
/// ... |           |           | ...    primitive param[0] (coarse face index)
/// ... |     p0    |     p1    | ...    primitive param[1] (patch param 0)
/// ... |           |           | ...    primitive param[2] (patch param 1)
/// ----+-----------+-----------+------
///
class _OsdIndexComputation final : public HdComputedBufferSource
{
    struct BaseFaceInfo
    {
        int baseFaceParam;
        GfVec2i baseFaceEdgeIndices;
    };

public:
    _OsdIndexComputation(HydraPassthroughMeshTopology *topology,
                         HdBufferSourceSharedPtr const &osdTopology);
    bool Resolve() override;
    bool HasChainedBuffer() const override;
    void GetBufferSpecs(HdBufferSpecVector *specs) const override;
    HdBufferSourceSharedPtrVector GetChainedBuffers() const override;

private:
    bool _CheckValid() const override;

    void _PopulateUniformPrimitiveBuffer(
        OpenSubdiv::Far::PatchTable const *patchTable);
    void _PopulatePatchPrimitiveBuffer(
        OpenSubdiv::Far::PatchTable const *patchTable);
    void _CreateBaseFaceMapping(
        std::vector<BaseFaceInfo> *result);

    HydraPassthroughMeshTopology *_topology;
    HdBufferSourceSharedPtr _osdTopology;
    HdBufferSourceSharedPtr _primitiveBuffer;
    HdBufferSourceSharedPtr _edgeIndicesBuffer;
};

_OsdIndexComputation::_OsdIndexComputation(
    HydraPassthroughMeshTopology *topology,
    HdBufferSourceSharedPtr const &osdTopology)
    : _topology(topology)
    , _osdTopology(osdTopology)
{
}

bool
_OsdIndexComputation::Resolve()
{
    using namespace OpenSubdiv;

    if (_osdTopology && !_osdTopology->IsResolved()) return false;

    if (!_TryLock()) return false;

    HydraPassthroughSubdivision * subdivision = _topology->GetSubdivision();
    if (!TF_VERIFY(subdivision)) {
        _SetResolved();
        return true;
    }

    OpenSubdiv::Far::PatchTable const * patchTable =
                                            subdivision->GetPatchTable();

    Far::Index const *firstIndex = NULL;
    size_t ptableSize = 0;
    if (patchTable) {
        ptableSize = patchTable->GetPatchControlVerticesTable().size();
        if (ptableSize > 0) {
            firstIndex = &patchTable->GetPatchControlVerticesTable()[0];
        }
    }

    TfToken const & scheme = _topology->GetScheme();

    if (_topology->RefinesToBSplinePatches() ||
        _topology->RefinesToBoxSplineTrianglePatches()) {

        // Bundle groups of 12 or 16 patch control vertices.
        int const arraySize = (ptableSize > 0)
            ? patchTable->GetPatchArrayDescriptor(0).GetNumControlVertices()
            : 0;

        VtArray<int> indices(ptableSize);
        memcpy(indices.data(), firstIndex, ptableSize * sizeof(int));

        HdBufferSourceSharedPtr patchIndices =
            std::make_shared<HdVtBufferSource>(
                HdTokens->indices, VtValue(indices), arraySize);

        _SetResult(patchIndices);

        _PopulatePatchPrimitiveBuffer(patchTable);

    } else if (HydraPassthroughSubdivision::RefinesToTriangles(scheme)) {
        // populate refined triangle indices.
        VtArray<GfVec3i> indices(ptableSize/3);
        memcpy(reinterpret_cast<Far::Index*>(indices.data()),
                firstIndex, ptableSize * sizeof(int));

        HdBufferSourceSharedPtr triIndices =
            std::make_shared<HdVtBufferSource>(
                HdTokens->indices, VtValue(indices));
        _SetResult(triIndices);

        _PopulateUniformPrimitiveBuffer(patchTable);
    } else {
        // populate refined quad indices.
        size_t const numQuads = ptableSize / 4;

        int const numIndicesPerQuad =
            _topology->TriangulateQuads()
                ? HdMeshTriQuadBuilder::NumIndicesPerTriQuad
                : HdMeshTriQuadBuilder::NumIndicesPerQuad;
        VtIntArray indices(numQuads * numIndicesPerQuad);

        if (numIndicesPerQuad == 4) {
            memcpy(indices.data(), firstIndex, ptableSize * sizeof(int));
        } else {
            HdMeshTriQuadBuilder outputIndices(indices.data(), true);
            for (size_t i=0; i<numQuads; ++i) {
                GfVec4i quadIndices(&firstIndex[i*4]);
                outputIndices.EmitQuadFace(quadIndices);
            }
        }

        // refined quads index buffer
        HdBufferSourceSharedPtr quadIndices =
            std::make_shared<HdVtBufferSource>(
                HdTokens->indices, VtValue(indices), numIndicesPerQuad);
        _SetResult(quadIndices);

        _PopulateUniformPrimitiveBuffer(patchTable);
    }

    _SetResolved();

    return true;
}

void
_OsdIndexComputation::_CreateBaseFaceMapping(
    std::vector<_OsdIndexComputation::BaseFaceInfo> *result)
{
    HD_TRACE_FUNCTION();
    HF_MALLOC_TAG_FUNCTION();

    if (!TF_VERIFY(result)) return;

    int const * numVertsPtr  = _topology->GetFaceVertexCounts().cdata();
    int const numFaces       = _topology->GetFaceVertexCounts().size();
    int const numVertIndices = _topology->GetFaceVertexIndices().size();

    result->clear();
    result->reserve(numFaces);

    int regFaceSize = 4;
    if (HydraPassthroughSubdivision::RefinesToTriangles( _topology->GetScheme() )) {
        regFaceSize = 3;
    }

    for (int i = 0, v = 0, ev = 0; i<numFaces; ++i) {
        int const nv = numVertsPtr[i];

        if (v+nv > numVertIndices) break;

        if (nv == regFaceSize) {
            BaseFaceInfo info;
            info.baseFaceParam =
                HdMeshUtil::EncodeCoarseFaceParam(i, /*edgeFlag=*/0);
            info.baseFaceEdgeIndices = GfVec2i(ev, 0);
            result->push_back(info);
        } else if (nv < 3) {
            int const numBaseFaces = (regFaceSize == 4) ? nv : nv - 2;
            for (int f = 0; f < numBaseFaces; ++f) {
                BaseFaceInfo info;
                info.baseFaceParam =
                    HdMeshUtil::EncodeCoarseFaceParam(i, /*edgeFlag=*/0);
                info.baseFaceEdgeIndices = GfVec2i(-1, -1);
                result->push_back(info);
            }
        } else {
            for (int j = 0; j < nv; ++j) {
                int edgeFlag = 0;
                if (j == 0) {
                    edgeFlag = 1;
                } else if (j == nv - 1) {
                    edgeFlag = 2;
                } else {
                    edgeFlag = 3;
                }

                BaseFaceInfo info;
                info.baseFaceParam =
                    HdMeshUtil::EncodeCoarseFaceParam(i, edgeFlag);
                info.baseFaceEdgeIndices = GfVec2i(ev+j, ev+(j+nv-1)%nv);
                result->push_back(info);
            }
        }

        v += nv;
        ev += nv;
    }

    result->shrink_to_fit();
}

void
_OsdIndexComputation::_PopulateUniformPrimitiveBuffer(
    OpenSubdiv::Far::PatchTable const * patchTable)
{
    HD_TRACE_FUNCTION();
    HF_MALLOC_TAG_FUNCTION();

    std::vector<BaseFaceInfo> patchFaceToBaseFaceMapping;
    _CreateBaseFaceMapping(&patchFaceToBaseFaceMapping);

    size_t numPatches = patchTable
        ? patchTable->GetPatchParamTable().size()
        : 0;
    VtVec3iArray primitiveParam(numPatches);
    VtVec2iArray edgeIndices(numPatches);

    for (size_t i = 0; i < numPatches; ++i) {
        OpenSubdiv::Far::PatchParam const &patchParam =
            patchTable->GetPatchParamTable()[i];

        int patchFaceIndex = patchParam.GetFaceId();
        BaseFaceInfo const & info = patchFaceToBaseFaceMapping[patchFaceIndex];

        unsigned int field0 = patchParam.field0;
        unsigned int field1 = patchParam.field1;
        primitiveParam[i][0] = info.baseFaceParam;
        primitiveParam[i][1] = *((int*)&field0);
        primitiveParam[i][2] = *((int*)&field1);

        edgeIndices[i] = info.baseFaceEdgeIndices;
    }

    _primitiveBuffer.reset(new HdVtBufferSource(
                               HdTokens->primitiveParam,
                               VtValue(primitiveParam)));

    _edgeIndicesBuffer.reset(new HdVtBufferSource(
                           HdTokens->edgeIndices,
                           VtValue(edgeIndices)));

}

void
_OsdIndexComputation::_PopulatePatchPrimitiveBuffer(
    OpenSubdiv::Far::PatchTable const * patchTable)
{
    HD_TRACE_FUNCTION();
    HF_MALLOC_TAG_FUNCTION();

    std::vector<BaseFaceInfo> patchFaceToBaseFaceMapping;
    _CreateBaseFaceMapping(&patchFaceToBaseFaceMapping);

    size_t numPatches = patchTable
        ? patchTable->GetPatchParamTable().size()
        : 0;
    VtVec4iArray primitiveParam(numPatches);
    VtVec2iArray edgeIndices(numPatches);

    for (size_t i = 0; i < numPatches; ++i) {
        OpenSubdiv::Far::PatchParam const &patchParam =
            patchTable->GetPatchParamTable()[i];

        float sharpness = 0.0;
        if (i < patchTable->GetSharpnessIndexTable().size()) {
            OpenSubdiv::Far::Index sharpnessIndex =
                patchTable->GetSharpnessIndexTable()[i];
            if (sharpnessIndex >= 0)
                sharpness = patchTable->GetSharpnessValues()[sharpnessIndex];
        }

        int patchFaceIndex = patchParam.GetFaceId();
        BaseFaceInfo const & info = patchFaceToBaseFaceMapping[patchFaceIndex];

        unsigned int field0 = patchParam.field0;
        unsigned int field1 = patchParam.field1;
        primitiveParam[i][0] = info.baseFaceParam;
        primitiveParam[i][1] = *((int*)&field0);
        primitiveParam[i][2] = *((int*)&field1);

        int sharpnessAsInt = static_cast<int>(sharpness);
        primitiveParam[i][3] = sharpnessAsInt;

        edgeIndices[i] = info.baseFaceEdgeIndices;
    }
    _primitiveBuffer.reset(new HdVtBufferSource(
                               HdTokens->primitiveParam,
                               VtValue(primitiveParam)));

    _edgeIndicesBuffer.reset(new HdVtBufferSource(
                           HdTokens->edgeIndices,
                           VtValue(edgeIndices)));
}

void
_OsdIndexComputation::GetBufferSpecs(HdBufferSpecVector *specs) const
{
    if (_topology->RefinesToBSplinePatches()) {
        // bi-cubic bspline patches
        specs->emplace_back(HdTokens->indices,
                            HdTupleType {HdTypeInt32, 16});
        // 3+1 (includes sharpness)
        specs->emplace_back(HdTokens->primitiveParam,
                            HdTupleType {HdTypeInt32Vec4, 1});
        specs->emplace_back(HdTokens->edgeIndices,
                            HdTupleType {HdTypeInt32Vec2, 1});
    } else if (_topology->RefinesToBoxSplineTrianglePatches()) {
        // quartic box spline triangle patches
        specs->emplace_back(HdTokens->indices,
                            HdTupleType {HdTypeInt32, 12});
        // 3+1 (includes sharpness)
        specs->emplace_back(HdTokens->primitiveParam,
                            HdTupleType {HdTypeInt32Vec4, 1});
        // int will suffice, but this unifies it for all the cases
        specs->emplace_back(HdTokens->edgeIndices,
                            HdTupleType {HdTypeInt32Vec2, 1});
    } else if (HydraPassthroughSubdivision::RefinesToTriangles(_topology->GetScheme())) {
        // triangles (loop)
        specs->emplace_back(HdTokens->indices,
                            HdTupleType {HdTypeInt32Vec3, 1});
        specs->emplace_back(HdTokens->primitiveParam,
                            HdTupleType {HdTypeInt32Vec3, 1});
        // int will suffice, but this unifies it for all the cases
        specs->emplace_back(HdTokens->edgeIndices,
                            HdTupleType {HdTypeInt32Vec2, 1});
    } else {
        // quads (catmark, bilinear)
        if (_topology->TriangulateQuads()) {
            specs->emplace_back(HdTokens->indices,
                                HdTupleType {HdTypeInt32, 6});
        } else {
            specs->emplace_back(HdTokens->indices,
                                HdTupleType {HdTypeInt32, 4});
        }
        specs->emplace_back(HdTokens->primitiveParam,
                            HdTupleType {HdTypeInt32Vec3, 1});
        specs->emplace_back(HdTokens->edgeIndices,
                            HdTupleType {HdTypeInt32Vec2, 1});
    }
}

bool
_OsdIndexComputation::HasChainedBuffer() const
{
    return true;
}

HdBufferSourceSharedPtrVector
_OsdIndexComputation::GetChainedBuffers() const
{
    return { _primitiveBuffer, _edgeIndicesBuffer };
}

bool
_OsdIndexComputation::_CheckValid() const
{
    return true;
}

// ---------------------------------------------------------------------------
/// \class _OsdRefineComputationCPU
///
/// OpenSubdiv CPU Refinement. Takes coarse vertex primvar data and applies the
/// OpenSubdiv stencils to produce refined vertex primvar data.
///
/// This class isn't inherited from HdComputedBufferSource.
/// GetData() returns the internal buffer to skip unnecessary copy.
///
class _OsdRefineComputationCPU final : public HdBufferSource
{
public:
    _OsdRefineComputationCPU(HydraPassthroughMeshTopology *topology,
                            HdBufferSourceSharedPtr const &source,
                            HdBufferSourceSharedPtr const &osdTopology,
                            HdInterpolation interpolation,
                            int fvarChannel = 0);
    ~_OsdRefineComputationCPU() override;

    TfToken const &GetName() const override;
    size_t ComputeHash() const override;
    void const* GetData() const override;
    HdTupleType GetTupleType() const override;
    size_t GetNumElements() const override;
    void GetBufferSpecs(HdBufferSpecVector *specs) const override;
    bool Resolve() override;
    bool HasPreChainedBuffer() const override;
    HdBufferSourceSharedPtr GetPreChainedBuffer() const override;
    HdInterpolation GetInterpolation() const;

protected:
    bool _CheckValid() const override;

private:
    HydraPassthroughMeshTopology *_topology;
    HdBufferSourceSharedPtr _source;
    HdBufferSourceSharedPtr _osdTopology;
    std::vector<float> _primvarBuffer;
    HdInterpolation _interpolation;
    int _fvarChannel;
};

_OsdRefineComputationCPU::_OsdRefineComputationCPU(
    HydraPassthroughMeshTopology *topology,
    HdBufferSourceSharedPtr const &source,
    HdBufferSourceSharedPtr const &osdTopology,
    HdInterpolation interpolation,
    int fvarChannel)
    : _topology(topology)
    , _source(source)
    , _osdTopology(osdTopology)
    , _interpolation(interpolation)
    , _fvarChannel(fvarChannel)
{
}

_OsdRefineComputationCPU::~_OsdRefineComputationCPU() = default;

TfToken const &
_OsdRefineComputationCPU::GetName() const
{
    return _source->GetName();
}

template <class HashState>
void TfHashAppend(HashState &h,
                  _OsdRefineComputationCPU const &bs)
{
    h.Append(bs.GetInterpolation());
}

size_t
_OsdRefineComputationCPU::ComputeHash() const
{
    return TfHash()(*this);
}

void const *
_OsdRefineComputationCPU::GetData() const
{
    return _primvarBuffer.data();
}

HdTupleType
_OsdRefineComputationCPU::GetTupleType() const
{
    return _source->GetTupleType();
}

size_t
_OsdRefineComputationCPU::GetNumElements() const
{
    // Stride is measured here in components, not bytes.
    size_t const elementStride =
        HdGetComponentCount(_source->GetTupleType().type);
    return _primvarBuffer.size() / elementStride;
}

HdInterpolation
_OsdRefineComputationCPU::GetInterpolation() const
{
    return _interpolation;
}

bool
_OsdRefineComputationCPU::Resolve()
{
    if (_source && !_source->IsResolved()) return false;
    if (_osdTopology && !_osdTopology->IsResolved()) return false;

    if (!_TryLock()) return false;

    HD_TRACE_FUNCTION();
    HF_MALLOC_TAG_FUNCTION();

    auto *subdivision = _topology->GetSubdivision();
    if (!TF_VERIFY(subdivision)) {
        _SetResolved();
        return true;
    }

    // prepare cpu vertex buffer including refined vertices
    subdivision->RefineCPU(_source,
                           &_primvarBuffer,
                           _interpolation,
                           _fvarChannel);

    HD_PERF_COUNTER_INCR(HdPerfTokens->subdivisionRefineCPU);

    _SetResolved();
    return true;
}

bool
_OsdRefineComputationCPU::_CheckValid() const
{
    bool valid = _source->IsValid();

    // _osdTopology is optional
    valid &= _osdTopology ? _osdTopology->IsValid() : true;

    return valid;
}

void
_OsdRefineComputationCPU::GetBufferSpecs(HdBufferSpecVector *specs) const
{
    // produces same buffer specs as source
    _source->GetBufferSpecs(specs);
}

bool
_OsdRefineComputationCPU::HasPreChainedBuffer() const
{
    return true;
}

HdBufferSourceSharedPtr
_OsdRefineComputationCPU::GetPreChainedBuffer() const
{
    return _source;
}

// ---------------------------------------------------------------------------
/// \class _OsdFvarIndexComputation
///
/// The equivelent of _OsdIndexComputation for face-varying data. Computes
/// a refined index buffer for a channel of face varying primvars. Each primvar
/// in that channel can share this index buffer.
class _OsdFvarIndexComputation final : public HdComputedBufferSource
{

public:
    _OsdFvarIndexComputation(HydraPassthroughMeshTopology *topology,
                             HdBufferSourceSharedPtr const &osdTopology,
                             int channel);
    bool HasChainedBuffer() const override;
    bool Resolve() override;
    void GetBufferSpecs(HdBufferSpecVector *specs) const override;
    HdBufferSourceSharedPtrVector GetChainedBuffers() const override;

protected:
    bool _CheckValid() const override;

private:
    void _PopulateFvarPatchParamBuffer(
        OpenSubdiv::Far::PatchTable const *patchTable);

    HydraPassthroughMeshTopology *_topology;
    HdBufferSourceSharedPtr _osdTopology;
    HdBufferSourceSharedPtr _fvarPatchParamBuffer;
    int _channel;
    TfToken _indicesName;
    TfToken _patchParamName;
};

_OsdFvarIndexComputation::_OsdFvarIndexComputation (
    HydraPassthroughMeshTopology *topology,
    HdBufferSourceSharedPtr const &osdTopology,
    int channel)
    : _topology(topology)
    , _osdTopology(osdTopology)
    , _channel(channel)
{
    const std::string channelStr = std::to_string(_channel);
    _indicesName = TfToken(
        HydraPassthroughSubdivision::PrimvarChannelIndexBaseName + channelStr);
    _patchParamName = TfToken(
         HydraPassthroughSubdivision::FvarPatchParamBaseName + channelStr);
}

bool
_OsdFvarIndexComputation::Resolve()
{
    using namespace OpenSubdiv;

    if (_osdTopology && !_osdTopology->IsResolved()) return false;

    if (!_TryLock()) return false;

    HydraPassthroughSubdivision * subdivision = _topology->GetSubdivision();
    if (!TF_VERIFY(subdivision)) {
        _SetResolved();
        return true;
    }

    Far::PatchTable const * patchTable = subdivision->GetPatchTable();
    size_t const numPatches = patchTable ? patchTable->GetNumPatchesTotal() : 0;

    VtIntArray fvarIndices = subdivision->GetRefinedFvarIndices(_channel);
    Far::Index const * firstIndex =
        !fvarIndices.empty() ? fvarIndices.cdata() : nullptr;

    TfToken const & scheme = _topology->GetScheme();

    if (_topology->RefinesToBSplinePatches() ||
        _topology->RefinesToBoxSplineTrianglePatches()) {

        // Bundle groups of 12 or 16 patch control vertices
        int const arraySize = (numPatches > 0) ?
            patchTable->GetFVarPatchDescriptor(_channel).GetNumControlVertices()
            : 0;

        VtIntArray indices(arraySize * numPatches);
        memcpy(indices.data(), firstIndex,
               arraySize * numPatches * sizeof(int));

        HdBufferSourceSharedPtr patchIndices =
            std::make_shared<HdVtBufferSource>(
                _indicesName, VtValue(indices), arraySize);

        _SetResult(patchIndices);
        _PopulateFvarPatchParamBuffer(patchTable);
    } else if (HydraPassthroughSubdivision::RefinesToTriangles(scheme)) {
        // populate refined triangle indices.
        VtArray<GfVec3i> indices(numPatches);
        memcpy(reinterpret_cast<Far::Index*>(indices.data()),
                firstIndex, 3 * numPatches * sizeof(int));

        HdBufferSourceSharedPtr triIndices =
            std::make_shared<HdVtBufferSource>(_indicesName, VtValue(indices));
        _SetResult(triIndices);
    } else {
        // populate refined quad indices.
        VtArray<GfVec4i> indices(numPatches);
        memcpy(reinterpret_cast<Far::Index*>(indices.data()),
                firstIndex, 4 * numPatches * sizeof(int));

        HdBufferSourceSharedPtr quadIndices =
            std::make_shared<HdVtBufferSource>(_indicesName, VtValue(indices));
        _SetResult(quadIndices);
    }

    _SetResolved();
    return true;
}

void
_OsdFvarIndexComputation::_PopulateFvarPatchParamBuffer(
    OpenSubdiv::Far::PatchTable const * patchTable)
{
    HD_TRACE_FUNCTION();
    HF_MALLOC_TAG_FUNCTION();

    VtVec2iArray fvarPatchParam(0);

    if (patchTable) {
        OpenSubdiv::Far::ConstPatchParamArray patchParamArray =
            patchTable->GetFVarPatchParams(_channel);
        size_t numPatches = patchParamArray.size();
        fvarPatchParam.resize(numPatches);

        for (size_t i = 0; i < numPatches; ++i) {
            OpenSubdiv::Far::PatchParam const &patchParam = patchParamArray[i];
            fvarPatchParam[i][0] = patchParam.field0;
            fvarPatchParam[i][1] = patchParam.field1;
        }
    }

    _fvarPatchParamBuffer.reset(new HdVtBufferSource(
                                    _patchParamName, VtValue(fvarPatchParam)));
}

void
_OsdFvarIndexComputation::GetBufferSpecs(HdBufferSpecVector *specs) const
{
    if (_topology->RefinesToBSplinePatches()) {
        // bi-cubic bspline patches
        specs->emplace_back(_indicesName, HdTupleType {HdTypeInt32, 16});
        specs->emplace_back(_patchParamName, HdTupleType {HdTypeInt32Vec2, 1});
    } else if (_topology->RefinesToBoxSplineTrianglePatches()) {
        // quartic box spline triangle patches
        specs->emplace_back(_indicesName, HdTupleType {HdTypeInt32, 12});
        specs->emplace_back(_patchParamName, HdTupleType {HdTypeInt32Vec2, 1});
    } else if (HydraPassthroughSubdivision::RefinesToTriangles(_topology->GetScheme())) {
        // triangles (loop)
        specs->emplace_back(_indicesName, HdTupleType {HdTypeInt32Vec3, 1});
    } else {
        // quads (catmark, bilinear)
        specs->emplace_back(_indicesName, HdTupleType {HdTypeInt32Vec4, 1});
    }
}

bool
_OsdFvarIndexComputation::HasChainedBuffer() const
{
    return (_topology->RefinesToBSplinePatches() ||
            _topology->RefinesToBoxSplineTrianglePatches());
}

HdBufferSourceSharedPtrVector
_OsdFvarIndexComputation::GetChainedBuffers() const
{
    if (_topology->RefinesToBSplinePatches() ||
        _topology->RefinesToBoxSplineTrianglePatches()) {
        return { _fvarPatchParamBuffer };
    } else {
        return {};
    }
}

bool
_OsdFvarIndexComputation::_CheckValid() const {
    return true;
}

// ---------------------------------------------------------------------------
// CPU stencil evaluation function
//
// This is what converts the course primvar data (points) into the refined
// data (patch control vertices) using the stencils generated by OpenSubdiv.
void
_EvalStencilsCPU(
    std::vector<float> * primvarBuffer,
    int const elementStride,
    int const numCoarsePoints,
    int const numRefinedPoints,
    std::vector<int> const & sizes,
    std::vector<int> const & offsets,
    std::vector<int> const & indices,
    std::vector<float> const  & weights)
{
    int const numElements = elementStride;
    std::vector<float> dst(numElements);

    for (int pointIndex = 0; pointIndex < numRefinedPoints; ++pointIndex) {
        for (int element = 0; element < numElements; ++element) {
            dst[element] = 0;
        }

        int const stencilSize = sizes[pointIndex];
        int const stencilOffset = offsets[pointIndex];

        for (int stencil = 0; stencil < stencilSize; ++stencil) {
            int const index = indices[stencil + stencilOffset];
            float const weight = weights[stencil + stencilOffset];
            int const srcIndex = index * elementStride;
            for (int element = 0; element < numElements; ++element) {
                dst[element] += weight * (*primvarBuffer)[srcIndex + element];
            }
        }

        int const dstIndex = (pointIndex + numCoarsePoints) * elementStride;
        for (int element = 0; element < numElements; ++element) {
            (*primvarBuffer)[dstIndex + element] = dst[element];
        }
    }
}

} // namespace

HydraPassthroughSubdivision::HydraPassthroughSubdivision(int refineLevel)
    : _refineLevel(refineLevel)
{
}

HydraPassthroughSubdivision::~HydraPassthroughSubdivision() = default;

/*static*/
bool
HydraPassthroughSubdivision::RefinesToTriangles(TfToken const &scheme)
{
    if (scheme == PxOsdOpenSubdivTokens->loop) {
        return true;
    }
    return false;
}

/*static*/
bool
HydraPassthroughSubdivision::RefinesToBSplinePatches(TfToken const &scheme)
{
    return scheme == PxOsdOpenSubdivTokens->catmullClark;
}

/*static*/
bool
HydraPassthroughSubdivision::RefinesToBoxSplineTrianglePatches(TfToken const &scheme)
{
#if OPENSUBDIV_VERSION_NUMBER >= 30400
    // v3.4.0 added support for limit surface patches for loop meshes
    if (scheme == PxOsdOpenSubdivTokens->loop) {
        return true;
    }
#endif
    return false;
}

/*static*/
int 
HydraPassthroughSubdivision::GetChannelFromPrimvarChannelIndexName(const TfToken &name) 
{
    std::string nameStr = name.GetString();
    std::string prefix = PrimvarChannelIndexBaseName;
    if (nameStr.rfind(prefix, 0) == 0) {
        std::string channelStr = nameStr.substr(prefix.size());
        try {
            return std::stoi(channelStr);
        } catch (const std::exception& e) {
            TF_RUNTIME_ERROR("Failed to extract face varying channel from primvar name %s: %s", name.GetText(), e.what());
            return -1;
        }
    }
    return -1;
}



OpenSubdiv::Far::StencilTable const *
HydraPassthroughSubdivision::GetStencilTable(
    HdInterpolation interpolation,
    int fvarChannel) const
{
    if (interpolation == HdInterpolationFaceVarying) {
        if (!TF_VERIFY(fvarChannel >= 0)) {
            return nullptr;
        }

        if (!TF_VERIFY(fvarChannel < (int)_faceVaryingStencils.size())) {
            return nullptr;
        }
    }

    return (interpolation == HdInterpolationVertex) ?
               _vertexStencils.get() :
           (interpolation == HdInterpolationVarying) ?
               _varyingStencils.get() :
           _faceVaryingStencils[fvarChannel].get();
}

HdBufferSourceSharedPtr
HydraPassthroughSubdivision::CreateTopologyComputation(
        HydraPassthroughMeshTopology *topology,
        SdfPath const &id)
{
    return std::make_shared<_OsdTopologyComputation>(topology, id);
}

HdBufferSourceSharedPtr
HydraPassthroughSubdivision::CreateIndexComputation(HydraPassthroughMeshTopology *topology,
    HdBufferSourceSharedPtr const &osdTopology)
{
    return std::make_shared<_OsdIndexComputation>(topology, osdTopology);
}

HdBufferSourceSharedPtr
HydraPassthroughSubdivision::CreateFvarIndexComputation(
    HydraPassthroughMeshTopology *topology,
    HdBufferSourceSharedPtr const &osdTopology,
    int channel)
{
    return std::make_shared<_OsdFvarIndexComputation>(
        topology, osdTopology, channel);
}

HdBufferSourceSharedPtr
HydraPassthroughSubdivision::CreateRefineComputationCPU(
    HydraPassthroughMeshTopology *topology,
    HdBufferSourceSharedPtr const &source,
    HdBufferSourceSharedPtr const &osdTopology,
    HdInterpolation interpolation,
    int fvarChannel)
{
    return std::make_shared<_OsdRefineComputationCPU>(
        topology, source, osdTopology, interpolation, fvarChannel);
}

void
HydraPassthroughSubdivision::SetRefinementTables(
    std::unique_ptr<OpenSubdiv::Far::StencilTable const> && vertexStencils,
    std::unique_ptr<OpenSubdiv::Far::StencilTable const> && varyingStencils,
    std::vector<std::unique_ptr<
        OpenSubdiv::Far::StencilTable const>> && faceVaryingStencils,
    std::unique_ptr<OpenSubdiv::Far::PatchTable const> && patchTable)
{
    _vertexStencils = std::move(vertexStencils);
    _varyingStencils = std::move(varyingStencils);

    _faceVaryingStencils.resize(faceVaryingStencils.size());
    for (size_t i = 0; i < _faceVaryingStencils.size(); ++i) {
        _faceVaryingStencils[i] = std::move(faceVaryingStencils[i]);
    }

    _patchTable = std::move(patchTable);
}

void
HydraPassthroughSubdivision::RefineCPU(
        HdBufferSourceSharedPtr const & source,
        std::vector<float> * primvarBuffer,
        HdInterpolation interpolation,
        int fvarChannel)
{
    HD_TRACE_FUNCTION();
    HF_MALLOC_TAG_FUNCTION();

    OpenSubdiv::Far::StencilTable const * stencilTable =
        GetStencilTable(interpolation, fvarChannel);

    if (!TF_VERIFY(stencilTable)) return;

    // if there is no stencil (e.g. torus with adaptive refinement),
    // just return here
    if (stencilTable->GetNumStencils() == 0) return;

    // Stride is measured here in components, not bytes.
    size_t const elementStride =
        HdGetComponentCount(source->GetTupleType().type);

    size_t const numTotalElements =
        stencilTable->GetNumControlVertices() + stencilTable->GetNumStencils();
    primvarBuffer->resize(numTotalElements * elementStride);

    // if the mesh has more vertices than that in use in topology,
    // we need to trim the buffer so that they won't overrun the coarse
    // vertex buffer which we allocated using the stencil table.
    // see HydraPassthroughSubdivision::GetNumVertices()
    size_t numSrcElements = source->GetNumElements();
    if (numSrcElements > (size_t)stencilTable->GetNumControlVertices()) {
        numSrcElements = stencilTable->GetNumControlVertices();
    }

    float const * srcData = static_cast<float const *>(source->GetData());
    std::copy(srcData, srcData + (numSrcElements * elementStride),
              primvarBuffer->begin());

    _EvalStencilsCPU(
        primvarBuffer,
        elementStride,
        stencilTable->GetNumControlVertices(),
        stencilTable->GetNumStencils(),
        stencilTable->GetSizes(),
        stencilTable->GetOffsets(),
        stencilTable->GetControlIndices(),
        stencilTable->GetWeights()
    );
}

VtIntArray
HydraPassthroughSubdivision::GetRefinedFvarIndices(int channel) const
{
    VtIntArray fvarIndices;
    if (_patchTable && _patchTable->GetNumFVarChannels() > channel) {
        OpenSubdiv::Far::ConstIndexArray indices =
            _patchTable->GetFVarValues(channel);
        for (int i = 0; i < indices.size(); ++i) {
            fvarIndices.push_back(indices[i]);
        }
    }
    return fvarIndices;
}

PXR_NAMESPACE_CLOSE_SCOPE
