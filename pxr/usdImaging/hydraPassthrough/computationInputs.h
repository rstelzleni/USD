#ifndef PXR_USD_IMAGING_HYDRA_PASSTHROUGH_COMPUTATION_INPUTS_H
#define PXR_USD_IMAGING_HYDRA_PASSTHROUGH_COMPUTATION_INPUTS_H

#include "pxr/pxr.h"

#include "pxr/imaging/hd/bufferSource.h"

#include <memory>

PXR_NAMESPACE_OPEN_SCOPE

using HydraPassthroughExtCompCpuComputationSharedPtr =
    std::shared_ptr<class HydraPassthroughExtCompCpuComputation>;

namespace HydraPassthroughComputationInputs
{

using ExtCompInputSourceSharedPtr =
    std::shared_ptr<class ExtCompInputSource>;
using ExtCompInputSourceSharedPtrVector =
    std::vector<ExtCompInputSourceSharedPtr>;

///
/// Abstract base class for a Buffer Source that represents a binding to an
/// input to an ExtComputation.
///
/// Analogous to HdSt_ExtCompInputSource
class ExtCompInputSource : public HdNullBufferSource {
public:
    /// Constructs the input binding with the name inputName
    ExtCompInputSource(const TfToken &inputName);

    ~ExtCompInputSource() override;

    /// Returns the name of the input.
    const TfToken &GetName() const override final;

    /// Returns the value associated with the input.
    virtual const VtValue &GetValue() const = 0;

private:
    TfToken _inputName;

    ExtCompInputSource() = delete;
    ExtCompInputSource(const ExtCompInputSource &) = delete;
    ExtCompInputSource &operator = (const ExtCompInputSource &) = delete;
};

///
/// An Hd Buffer Source Computation that is used to bind an ExtComputation input
/// to a value provided by the scene delegate.
///
/// Analogous to HdSt_ExtCompSceneInputSource
class ExtCompSceneInputSource final : public ExtCompInputSource
{
public:
    /// Constructs the computation, binding inputName to the provided value.
    ExtCompSceneInputSource(
        const TfToken &inputName, const VtValue &value);

    ~ExtCompSceneInputSource() override;

    /// Set the state of the computation to resolved and returns true.
    bool Resolve() override;

    /// Returns the value associated with this input.
    const VtValue &GetValue() const override;

protected:
    /// Returns if this computation binding is valid.
    bool _CheckValid() const override;

private:
    VtValue _value;

    // No copying, assignment or default construction.
    ExtCompSceneInputSource() = delete;
    ExtCompSceneInputSource(const ExtCompSceneInputSource &) = delete;
    ExtCompSceneInputSource &operator = (const ExtCompSceneInputSource &) = delete;
};

///
/// An Hd Buffer Source Computation that is used to bind an ExtComputation
/// input to a specific output of another ExtComputation.
///
/// Analogous to HdSt_ExtCompComputedInputSource
class ExtCompComputedInputSource final : public ExtCompInputSource
{
public:
    /// Constructs the computation, binding inputName to sourceOutputName
    /// on buffer source representation of the source computation.
    ExtCompComputedInputSource(
        const TfToken &inputName,
        const HydraPassthroughExtCompCpuComputationSharedPtr &source,
        const TfToken &sourceOutputName);

    ~ExtCompComputedInputSource() override;

    /// Returns true once the source computation has been resolved.
    bool Resolve() override;

    /// Obtains the value of the output from the source computation.
    const VtValue &GetValue() const override;

protected:
    /// Returns true if the binding is successful.
    bool _CheckValid() const override;

private:
    HydraPassthroughExtCompCpuComputationSharedPtr _source;
    size_t                                         _sourceOutputIdx;

    ExtCompComputedInputSource() = delete;
    ExtCompComputedInputSource(
        const ExtCompComputedInputSource &) = delete;
    ExtCompComputedInputSource &operator = (
        const ExtCompComputedInputSource &) = delete;
};


} // namespace HydraPassthroughComputationInputs

PXR_NAMESPACE_CLOSE_SCOPE

#endif // PXR_USD_IMAGING_HYDRA_PASSTHROUGH_COMPUTATION_INPUTS_H

