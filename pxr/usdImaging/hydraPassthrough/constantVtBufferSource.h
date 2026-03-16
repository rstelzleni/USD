#ifndef PXR_USD_IMAGING_HYDRA_PASSTHROUGH_CONSTANT_VT_BUFFER_SOURCE_H
#define PXR_USD_IMAGING_HYDRA_PASSTHROUGH_CONSTANT_VT_BUFFER_SOURCE_H

#include "pxr/pxr.h"
#include "pxr/imaging/hd/bufferSource.h"
#include "pxr/imaging/hd/types.h"

#include "pxr/base/tf/token.h"
#include "pxr/base/vt/value.h"

PXR_NAMESPACE_OPEN_SCOPE


/// \class HydraPassthroughConstantVtBufferSource
///
/// An implementation of HdBufferSource where the source data value is a
/// constant VtValue.
///
/// This is specifically for passing constant VtValues through to the render
/// data, even if there is no HdType for the value. This allows us to handle
/// strings, tokens, and other numeric types that HdType doesn't support. These
/// are often used for things like ids, tags, etc.
///
/// Note that this includes some numeric types that could also be supported by
/// non-constant primvars, so would need subdivision. int64 is one example. We
/// could probably support this, unless we run into issues with not having an
/// HdType enum in the hd code. Most of the computations take place in this
/// libs code, so that might not be an issue. One thing that might be an issue,
/// the subdivision code assumes arrays have fixed size entries, and I'm not
/// sure that's true for string and TfToken. If we need to support non-constant
/// primvars with these types in the future we can explore this. Since HdSt
/// hasn't needed it yet, it seems like it isn't urgent.
///
class HydraPassthroughConstantVtBufferSource final : public HdBufferSource
{
public:
    /// Constructs a new buffer from a VtValue.
    ///
    /// \param arraySize indicates how many values are provided per element.
    HydraPassthroughConstantVtBufferSource(TfToken const &name, VtValue const& value,
                     int arraySize=1);

    ~HydraPassthroughConstantVtBufferSource() override;

    const VtValue& GetValue() const;

    /// Return the name of this buffer source
    TfToken const &GetName() const override;

    /// Returns the raw pointer to the underlying data.
    void const* GetData() const override;

    /// Returns the data type and count of this buffer source.
    HdTupleType GetTupleType() const override;

    /// Returns the number of elements (e.g. VtVec3dArray().GetLength()) from
    /// the source array.
    size_t GetNumElements() const override;

    /// Add the buffer spec for this buffer source into given bufferspec vector.
    void GetBufferSpecs(HdBufferSpecVector *specs) const override;

    /// Prepare the access of GetData().
    bool Resolve() override;

protected:
    bool _CheckValid() const override;

private:
    TfToken _name;
    VtValue _value;
    HdTupleType _tupleType;
    size_t _numElements;
};


PXR_NAMESPACE_CLOSE_SCOPE

#endif //PXR_USD_IMAGING_HYDRA_PASSTHROUGH_CONSTANT_VT_BUFFER_SOURCE_H
