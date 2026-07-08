#include "computationUtil.h"

#include "extCompPrimvarBufferSource.h"
#include "computation.h"

PXR_NAMESPACE_OPEN_SCOPE


namespace HydraPassthroughComputationUtil
{

void
GetExtComputationPrimvarsComputations(
    SdfPath const &id,
    HdSceneDelegate *sceneDelegate,
    HdExtComputationPrimvarDescriptorVector const& allCompPrimvars,
    HdDirtyBits dirtyBits,
    HdBufferSourceSharedPtrVector *sources,
    HdBufferSourceSharedPtrVector *reserveOnlySources,
    HdBufferSourceSharedPtrVector *separateComputationSources,
    ComputationComputeQueuePairVector *computations)
{
    TF_VERIFY(sources);
    TF_VERIFY(reserveOnlySources);
    TF_VERIFY(separateComputationSources);
    TF_VERIFY(computations);

    HdRenderIndex &renderIndex = sceneDelegate->GetRenderIndex();

    // Group computation primvars by source computation
    typedef std::map<SdfPath, HdExtComputationPrimvarDescriptorVector>
                                                    CompPrimvarsByComputation;
    CompPrimvarsByComputation byComputation;
    for (HdExtComputationPrimvarDescriptor const & compPrimvar:
                                                        allCompPrimvars) {
        byComputation[compPrimvar.sourceComputationId].push_back(compPrimvar);
    }

    // Create computation primvar buffer sources by source computation
    for (CompPrimvarsByComputation::value_type it: byComputation) { 
        SdfPath const &computationId = it.first;
        HdExtComputationPrimvarDescriptorVector const &compPrimvars = it.second;

        HdExtComputation const * sourceComp =
            static_cast<HdExtComputation const *>(
                renderIndex.GetSprim(HdPrimTypeTokens->extComputation,
                                     computationId));
        if (!(sourceComp && sourceComp->GetElementCount() > 0)) {
            continue;
        }

        // Computations may carry a GPU kernel (e.g. UsdSkel skinning ships
        // GLSL for Storm) in addition to a CPU callback. We always evaluate
        // on the CPU: InvokeExtComputation routes to the computation's CPU
        // callback regardless of the kernel source. If a computation has no
        // CPU callback its outputs won't resolve and the primvar is dropped.
        std::shared_ptr<HydraPassthroughExtCompCpuComputation> cpuComputation;
        for (HdExtComputationPrimvarDescriptor const & compPrimvar:
                                                            compPrimvars) {

            if (HdChangeTracker::IsPrimvarDirty(dirtyBits, id,
                                                compPrimvar.name)) {

                if (!cpuComputation) {
                   // Create the computation for the first dirty primvar
                    cpuComputation =
                        HydraPassthroughExtCompCpuComputation::CreateComputation(
                            sceneDelegate,
                            *sourceComp,
                            separateComputationSources);
                }

                // Create a primvar buffer source for the computation.
                // Each primvar buffer source can use the same computation,
                // because we know that this comp is for these primvars.
                HdBufferSourceSharedPtr primvarBufferSource =
                    std::make_shared<HydraPassthroughExtCompPrimvarBufferSource>(
                        compPrimvar.name,
                        cpuComputation,
                        compPrimvar.sourceComputationOutputName,
                        compPrimvar.valueType);

                sources->push_back(primvarBufferSource);
            }
        }
    }
}

} // namespace HydraPassthroughComputationUtil

PXR_NAMESPACE_CLOSE_SCOPE
