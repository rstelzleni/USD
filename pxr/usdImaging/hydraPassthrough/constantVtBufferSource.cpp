#include "pxr/usdImaging/hydraPassthrough/constantVtBufferSource.h"

#include "pxr/base/vt/array.h"
#include "pxr/base/vt/types.h"

PXR_NAMESPACE_OPEN_SCOPE

HydraPassthroughConstantVtBufferSource::HydraPassthroughConstantVtBufferSource(
        TfToken const& name,
        VtValue const& value,
        int arraySize)
    : _name(name)
{
    _value = value;

    _tupleType.type = HdTypeInvalid;

    // For the common case of a default value that is an empty
    // VtArray<T>, interpret it as one T per element rather than
    // a zero-sized tuple.
    if (_value.IsArrayValued() && _value.GetArraySize() == 0) {
        _tupleType.count = 1;
        _numElements = 0;
        return;
    }

    _tupleType.count = _value.IsArrayValued() ? _value.GetArraySize() : 1;

    // Check that array size makes sense. Note that HdVtBufferSource doesn't
    // verify this.
    if (arraySize <= 0 || _tupleType.count % arraySize != 0) {
        TF_CODING_ERROR("Invalid arraySize %d for value with count %zu for buffer '%s'",
                        arraySize, _tupleType.count, _name.GetText());
        // Set to a empty state
        _tupleType.count = 1;
        _numElements = 0;
        return;
    }

    // Factor the VtArray length into numElements and tuple count.
    // VtArray is a 1D array and does not have multidimensional shape,
    // therefore it cannot distinguish the case of N values for M elements
    // from the case of 1 value for NM elements.  This is why
    // HydraPassthroughConstantVtBufferSource requires the caller to provide this context
    // via the arraySize argument, so it can apply that shape here.
    _numElements = _tupleType.count / arraySize;
    _tupleType.count = arraySize;
}

HydraPassthroughConstantVtBufferSource::~HydraPassthroughConstantVtBufferSource()
{
}

const VtValue&
HydraPassthroughConstantVtBufferSource::GetValue() const
{
    return _value;
}

TfToken const &
HydraPassthroughConstantVtBufferSource::GetName() const
{
    return _name;
}

void const* 
HydraPassthroughConstantVtBufferSource::GetData() const
{
    // HdGetValueData returns nullptr for unsupported types
    // We don't use this anywhere, so this stub should be ok
    return nullptr;
}

HdTupleType 
HydraPassthroughConstantVtBufferSource::GetTupleType() const
{
    return _tupleType;
}

size_t
HydraPassthroughConstantVtBufferSource::GetNumElements() const
{
    return _numElements;
}

void
HydraPassthroughConstantVtBufferSource::GetBufferSpecs(HdBufferSpecVector *specs) const
{
    specs->push_back(HdBufferSpec(_name, _tupleType));
}

bool
HydraPassthroughConstantVtBufferSource::Resolve()
{
    if (!_TryLock()) return false;

    // do nothing, this is just a passthrough container
    _SetResolved();
    return true;
}

bool
HydraPassthroughConstantVtBufferSource::_CheckValid() const
{
    return true;
}

PXR_NAMESPACE_CLOSE_SCOPE

