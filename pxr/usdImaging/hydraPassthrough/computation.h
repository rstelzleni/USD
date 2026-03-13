#ifndef PXR_USD_IMAGING_HYDRA_PASSTHROUGH_COMPUTATION_H
#define PXR_USD_IMAGING_HYDRA_PASSTHROUGH_COMPUTATION_H

#include "pxr/pxr.h"

#include "pxr/usdImaging/hydraPassthrough/computationInputs.h"
#include "pxr/imaging/hd/bufferSource.h"
#include "pxr/imaging/hd/sceneDelegate.h"

#include <memory>
#include <vector>

PXR_NAMESPACE_OPEN_SCOPE

using HydraPassthroughComputationSharedPtr = std::shared_ptr<class HydraPassthroughComputation>;
using HydraPassthroughComputationSharedPtrVector = std::vector<HydraPassthroughComputationSharedPtr>;

/// Mirrors HdStComputation. That class is geared towards GPU computations, so
/// we'll need a version that works on CPU only. This does not derive from
/// any Hd classes, it is a concept in this lib only, and is executed as
/// part of the HydraPassthroughResourceRegistry::Commit processing.
class HydraPassthroughComputation {
public:
    virtual ~HydraPassthroughComputation() = default;

    /// Execute computation.
    virtual void Execute() = 0;

    /// This function is needed as HydraPassthroughComputation shares a templatized
    /// interface with HdBufferSource.
    ///
    /// It is a check to see if the GetBufferSpecs would produce a valid result.
    bool IsValid() { return true; }
};

///
/// A Buffer Source that represents a CPU implementation of a ExtComputation.
///
/// The computation implements the basic: input->processing->output model
/// where the inputs are other buffer sources and processing happens during
/// resolve.
///
/// Outputs to a computation are in SOA form, so a computation may have
/// many outputs, but each output has the same number of elements in it.
class HydraPassthroughExtCompCpuComputation  : public HdNullBufferSource {
public:

    static const size_t INVALID_OUTPUT_INDEX;

    /// Constructs a new Cpu ExtComputation source.
    /// inputs provides a list of buffer sources that this computation
    /// requires.
    /// outputs is a list of outputs by names that the computation produces.
    ///
    /// Num elements specifies the number of elements in the output.
    ///
    /// sceneDelegate and id are used to callback to the scene delegate
    /// in order to invoke computation processing.
    HydraPassthroughExtCompCpuComputation(
        const SdfPath &id,
        const 
          HydraPassthroughComputationInputs::ExtCompInputSourceSharedPtrVector
            &inputs,
        const TfTokenVector &outputs,
        int numElements,
        HdSceneDelegate *sceneDelegate);

    ~HydraPassthroughExtCompCpuComputation() override;

    /// Create a CPU computation implementing the given abstract computation.
    /// The scene delegate identifies which delegate to pull scene inputs from.
    static std::shared_ptr<HydraPassthroughExtCompCpuComputation>
    CreateComputation(HdSceneDelegate *sceneDelegate,
                      const HdExtComputation &computation,
                      HdBufferSourceSharedPtrVector *computationSources);

    /// Returns the id for this computation as a token.
    TfToken const &GetName() const override;

    /// Ask the scene delegate to run the computation and captures the output
    /// signals.
    bool Resolve() override;

    /// Number of elements in each output array
    size_t GetNumElements() const override;

    /// Converts a output name token into an index.
    size_t GetOutputIndex(const TfToken &outputName) const;

    /// Returns the value of the specified output
    /// (after the computations been Resolved).
    const VtValue &GetOutputByIndex(size_t index) const;

protected:
    /// Returns if the computation is specified correctly.
    bool _CheckValid() const override;

private:
    SdfPath                                 _id;
    HydraPassthroughComputationInputs::ExtCompInputSourceSharedPtrVector 
                                            _inputs;
    TfTokenVector                           _outputs;
    size_t                                  _numElements;
    HdSceneDelegate                        *_sceneDelegate;

    std::vector<VtValue>                    _outputValues;

    HydraPassthroughExtCompCpuComputation() = delete;
    HydraPassthroughExtCompCpuComputation(
        const HydraPassthroughExtCompCpuComputation &) = delete;
    HydraPassthroughExtCompCpuComputation &operator = (
        const HydraPassthroughExtCompCpuComputation &) = delete;

};

PXR_NAMESPACE_CLOSE_SCOPE

#endif // PXR_USD_IMAGING_HYDRA_PASSTHROUGH_COMPUTATION_H

