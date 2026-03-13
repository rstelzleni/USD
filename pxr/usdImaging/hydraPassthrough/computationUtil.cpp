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

        if (!sourceComp->GetGpuKernelSource().empty()) {
            TF_WARN("GPU computations aren't supported in HydraPassthrough. "
                     "Computation '%s' will be ignored on <%s>",
                     computationId.GetText(), id.GetText());

            /* If we encounter one of these we need to figure out what to do,
             * pass along the kernel source and dependencies to the client?
             *
            HdStExtCompGpuComputationSharedPtr gpuComputation;
            for (HdExtComputationPrimvarDescriptor const & compPrimvar:
                                                                compPrimvars) {

                if (HdChangeTracker::IsPrimvarDirty(dirtyBits, id,
                                                    compPrimvar.name)) {

                    if (!gpuComputation) {
                       // Create the computation for the first dirty primvar
                        gpuComputation =
                            HdStExtCompGpuComputation::CreateGpuComputation(
                                sceneDelegate,
                                sourceComp,
                                compPrimvars);

                        // Assume there are no dependencies between ExtComp so
                        // put all of them in queue zero.
                        computations->emplace_back(
                            gpuComputation, HdStComputeQueueZero);
                    }

                    // Create a primvar buffer source for the computation
                    HdBufferSourceSharedPtr primvarBufferSource =
                        std::make_shared<HdStExtCompGpuPrimvarBufferSource>(
                            compPrimvar.name,
                            compPrimvar.valueType,
                            sourceComp->GetElementCount(),
                            sourceComp->GetId());

                    // Gpu primvar sources only need to reserve space
                    reserveOnlySources->push_back(primvarBufferSource);
                }
            }
                */

        } else {

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
}

} // namespace HydraPassthroughComputationUtil

PXR_NAMESPACE_CLOSE_SCOPE
