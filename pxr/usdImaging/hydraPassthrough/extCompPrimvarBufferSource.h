#ifndef PXR_USD_IMAGING_HYDRA_PASSTHROUGH_EXT_COMP_PRIMVAR_BUFFER_SOURCE_H
#define PXR_USD_IMAGING_HYDRA_PASSTHROUGH_EXT_COMP_PRIMVAR_BUFFER_SOURCE_H

#include "pxr/pxr.h"

#include "pxr/usdImaging/hydraPassthrough/computation.h"

#include "pxr/imaging/hd/bufferSource.h"
#include "pxr/imaging/hd/types.h"

#include "pxr/base/tf/token.h"

#include <memory>

PXR_NAMESPACE_OPEN_SCOPE

/// Hd Buffer Source that binds a primvar to a Ext Computation output.
///
/// Analogous to HdStExtCompPrimvarBufferSource
class HydraPassthroughExtCompPrimvarBufferSource final : public HdBufferSource
{
public:

    /// Constructs a new primvar buffer source called primvarName and
    /// binds it to the output called sourceOutputName from the
    /// computation identified by source.
    ///
    /// Default value provides type information for the primvar and may
    /// be used in the event of an error.
    HydraPassthroughExtCompPrimvarBufferSource(
        const TfToken &primvarName,
        const std::shared_ptr<HydraPassthroughExtCompCpuComputation> &source,
        const TfToken &sourceOutputName,
        const HdTupleType &valueType);

    ~HydraPassthroughExtCompPrimvarBufferSource() override;

    /// Returns the name of the primvar.
    TfToken const &GetName() const override;

    /// Adds this Primvar's buffer description to the buffer spec vector.
    void GetBufferSpecs(HdBufferSpecVector *specs) const override;

    /// Computes and returns a hash value for the underlying data.
    size_t ComputeHash() const override;

    /// Extracts the primvar from the source computation.
    bool Resolve() override;

    /// Returns a raw pointer to the primvar data.
    void const *GetData() const override;

    /// Returns the tuple data format of the primvar data.
    HdTupleType GetTupleType() const override;

    /// Returns a count of the number of elements.
    size_t GetNumElements() const override;

protected:
    /// Returns true if the binding to the source computation was successful.
    bool _CheckValid() const override;

private:
    // TfHash support.
    template <class HashState>
    friend void TfHashAppend(HashState &h,
                             HydraPassthroughExtCompPrimvarBufferSource const &);

    TfToken                            _primvarName;
    std::shared_ptr<HydraPassthroughExtCompCpuComputation> 
                                       _source;
    size_t                             _sourceOutputIdx;
    HdTupleType                        _tupleType;
    void const                        *_rawDataPtr;

    HydraPassthroughExtCompPrimvarBufferSource() = delete;
    HydraPassthroughExtCompPrimvarBufferSource(
        const HydraPassthroughExtCompPrimvarBufferSource &) = delete;
    HydraPassthroughExtCompPrimvarBufferSource &operator = (
        const HydraPassthroughExtCompPrimvarBufferSource &) = delete;
};


PXR_NAMESPACE_CLOSE_SCOPE

#endif // PXR_USD_IMAGING_HYDRA_PASSTHROUGH_EXT_COMP_PRIMVAR_BUFFER_SOURCE_H
