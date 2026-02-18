#include "computation.h"

#include "pxr/imaging/hd/extComputation.h"
#include "pxr/imaging/hd/extComputationContextInternal.h"
#include "pxr/imaging/hd/sceneDelegate.h"

#include <limits>


PXR_NAMESPACE_OPEN_SCOPE

namespace CompInputs = HydraPassthroughComputationInputs;

const size_t HydraPassthroughExtCompCpuComputation::INVALID_OUTPUT_INDEX =
                                        std::numeric_limits<size_t>::max();

HydraPassthroughExtCompCpuComputation::HydraPassthroughExtCompCpuComputation(
    const SdfPath &id,
    const CompInputs::ExtCompInputSourceSharedPtrVector &inputs,
    const TfTokenVector &outputs,
    int numElements,
    HdSceneDelegate *sceneDelegate)
    : HdNullBufferSource()
    , _id(id)
    , _inputs(inputs)
    , _outputs(outputs)
    , _numElements(numElements)
    , _sceneDelegate(sceneDelegate)
    , _outputValues()
{
}

HydraPassthroughExtCompCpuComputation::~HydraPassthroughExtCompCpuComputation() = default;

std::shared_ptr<HydraPassthroughExtCompCpuComputation>
HydraPassthroughExtCompCpuComputation::CreateComputation(
    HdSceneDelegate *sceneDelegate,
    const HdExtComputation &computation,
    HdBufferSourceSharedPtrVector *computationSources)
{
    HdRenderIndex &renderIndex = sceneDelegate->GetRenderIndex();

    const SdfPath &id = computation.GetId();

    // Get input values from the scene, and add them to the local list
    // of inputs to this computation, and to the collected computation
    // source vector passed in.
    CompInputs::ExtCompInputSourceSharedPtrVector inputs;
    for (const TfToken &inputName: computation.GetSceneInputNames()) {
        VtValue inputValue = sceneDelegate->GetExtComputationInput(
                                                id, inputName);
        auto inputSource =
            std::make_shared<CompInputs::ExtCompSceneInputSource>(
                                                inputName, inputValue);
        computationSources->push_back(inputSource);
        inputs.push_back(inputSource);
    }

    // Get input values from other computations, and add them to the local list
    // of inputs to this computation, and to the collected computation source
    // vector passed in.
    for (const HdExtComputationInputDescriptor & compInput:
                computation.GetComputationInputs()) {

        HdExtComputation const * sourceComp =
            static_cast<HdExtComputation const *>(
                renderIndex.GetSprim(HdPrimTypeTokens->extComputation,
                                     compInput.sourceComputationId));

        if (sourceComp != nullptr) {

            // Handle computations that just aggregate inputs
            if (sourceComp->IsInputAggregation()) {
                VtValue inputValue =
                    sceneDelegate->GetExtComputationInput(
                            compInput.sourceComputationId,
                            compInput.name);
                auto inputSource =
                    std::make_shared<CompInputs::ExtCompSceneInputSource>(
                                                compInput.name, inputValue);
                computationSources->push_back(inputSource);
                inputs.push_back(inputSource);
                continue;
            }

            // Recursively create computations for the input computations
            auto sourceComputation = CreateComputation(sceneDelegate,
                                                       *sourceComp,
                                                       computationSources);

            auto inputSource =
                std::make_shared<CompInputs::ExtCompComputedInputSource>(
                        compInput.name,
                        sourceComputation,
                        compInput.sourceComputationOutputName);

            computationSources->push_back(inputSource);
            inputs.push_back(inputSource);
        }
    }

    // The result is a new computation with these inputs
    auto result =
        std::make_shared<HydraPassthroughExtCompCpuComputation>(
                                        id,
                                        inputs,
                                        computation.GetOutputNames(),
                                        computation.GetElementCount(),
                                        sceneDelegate);

    // Add the result to the collected comp sources, along with all the
    // inputs from above
    computationSources->push_back(result);

    return result;
}

TfToken const &
HydraPassthroughExtCompCpuComputation::GetName() const
{
    return _id.GetToken();
}

bool
HydraPassthroughExtCompCpuComputation::Resolve()
{
    size_t numInputs = _inputs.size();

    bool inputError = false;
    for (size_t inputNum = 0; inputNum < numInputs; ++inputNum) {
        if (_inputs[inputNum]->IsValid()) {
            if (!_inputs[inputNum]->IsResolved()) {
                return false;
            }

            inputError |= _inputs[inputNum]->HasResolveError();
        }
        else
        {
            inputError = true;
        }
    }

    if (!_TryLock()) return false;

    if (inputError) {
         _SetResolveError();
         return true;
     }


    HdExtComputationContextInternal context;

    for (size_t inputNum = 0; inputNum < numInputs; ++inputNum) {
        const auto &input = _inputs[inputNum];
        context.SetInputValue(input->GetName(), input->GetValue());
    }

    _sceneDelegate->InvokeExtComputation(_id, &context);
    if (context.HasComputationError()) {
        _SetResolveError();
        return true;
    }


    size_t numOutputs = _outputs.size();

    _outputValues.resize(numOutputs);
    for (size_t outputNum = 0; outputNum < numOutputs; ++outputNum) {
        const TfToken &outputName = _outputs[outputNum];

        if (!context.GetOutputValue(outputName, &_outputValues[outputNum])) {
            _SetResolveError();
            return true;
        }
    }

    _SetResolved();
    return true;
}

size_t
HydraPassthroughExtCompCpuComputation::GetNumElements() const
{
    return _numElements;
}

size_t
HydraPassthroughExtCompCpuComputation::GetOutputIndex(const TfToken &outputName) const
{
    size_t numOutputs = _outputs.size();
    for (size_t outputNum = 0; outputNum < numOutputs; ++outputNum) {
        if (outputName == _outputs[outputNum]) {
            return outputNum;
        }
    }

    return INVALID_OUTPUT_INDEX;
}

const VtValue &
HydraPassthroughExtCompCpuComputation::GetOutputByIndex(size_t index) const
{
    return _outputValues[index];
}

bool
HydraPassthroughExtCompCpuComputation::_CheckValid() const
{
    return _sceneDelegate != nullptr;
}

PXR_NAMESPACE_CLOSE_SCOPE
