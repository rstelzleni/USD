#include "computationInputs.h"
#include "computation.h"

PXR_NAMESPACE_OPEN_SCOPE

namespace HydraPassthroughComputationInputs
{

// ------------------------------------------------------------------
// ExtCompInputSource

ExtCompInputSource::ExtCompInputSource(const TfToken &inputName)
    : HdNullBufferSource()
    , _inputName(inputName)
{

}

ExtCompInputSource::~ExtCompInputSource() = default;

TfToken const &
ExtCompInputSource::GetName() const
{
    return _inputName;
}

// ------------------------------------------------------------------
// ExtCompSceneInputSource

ExtCompSceneInputSource::ExtCompSceneInputSource(
    const TfToken &inputName,
    const VtValue &value)
    : ExtCompInputSource(inputName)
    , _value(value)
{
}

ExtCompSceneInputSource::~ExtCompSceneInputSource() = default;

bool
ExtCompSceneInputSource::Resolve()
{
    if (!_TryLock()) return false;

    _SetResolved();
    return true;
}

const VtValue &
ExtCompSceneInputSource::GetValue() const
{
    return _value;
}

bool
ExtCompSceneInputSource::_CheckValid() const
{
    return true;
}

// ------------------------------------------------------------------
// ExtCompComputedInputSource

ExtCompComputedInputSource::ExtCompComputedInputSource(
    const TfToken &name,
    const HydraPassthroughExtCompCpuComputationSharedPtr &source,
    const TfToken &sourceOutputName)
    : ExtCompInputSource(name)
    , _source(source)
    , _sourceOutputIdx(HydraPassthroughExtCompCpuComputation::INVALID_OUTPUT_INDEX)
{
    _sourceOutputIdx = source->GetOutputIndex(sourceOutputName);
}

ExtCompComputedInputSource::~ExtCompComputedInputSource() = default;

bool
ExtCompComputedInputSource::Resolve()
{
    bool sourceValid = _source->IsValid();
    if (sourceValid) {
        if (!_source->IsResolved()) {
            return false;
        }
    }

    if (!_TryLock()) return false;

    if (!sourceValid || _source->HasResolveError()) {
        _SetResolveError();
        return true;
    }

    _SetResolved();
    return true;
}

const VtValue &
ExtCompComputedInputSource::GetValue() const
{
    return _source->GetOutputByIndex(_sourceOutputIdx);
}

bool
ExtCompComputedInputSource::_CheckValid() const
{
    return (_source &&
            (_sourceOutputIdx !=
             HydraPassthroughExtCompCpuComputation::INVALID_OUTPUT_INDEX));
}


} // namespace HydraPassthroughComputationInputs

PXR_NAMESPACE_CLOSE_SCOPE
