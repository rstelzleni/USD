#include "primUtil.h"

#include "pxr/usdImaging/hydraPassthrough/resourceRegistry.h"
#include "pxr/imaging/hd/bufferSource.h"
#include "pxr/imaging/hd/vtBufferSource.h"

PXR_NAMESPACE_OPEN_SCOPE

namespace HydraPassthroughPrimUtil
{


// This function does almost nothing. Keeping it because HdSt uses it to filter
// primvars based on what's required to render. If we decide to do that, we can
// flesh this function out to match the HdSt/primUtils.cpp version.
// In particular, see _IsEnabledPrimvarFiltering, which requires the HdSt draw
// item's knowledge of its shader network.
HdPrimvarDescriptorVector
GetPrimvarDescriptors(
    HdRprim const * prim,
    HdSceneDelegate * delegate,
    HdInterpolation interpolation)
{
    HdPrimvarDescriptorVector primvars =
        prim->GetPrimvarDescriptors(delegate, interpolation);

    return primvars;
}

void
PopulateConstantPrimvars(
    HdRprim *prim,
    HdRprimSharedData *sharedData,
    HdSceneDelegate *delegate,
    HydraPassthroughResourceRegistry *resourceRegistry,
    HdDrawItem *drawItem,
    HdDirtyBits *dirtyBits,
    HdPrimvarDescriptorVector const& constantPrimvars,
    bool *hasMirroredTransform)
{
    HD_TRACE_FUNCTION();
    HF_MALLOC_TAG_FUNCTION();

    SdfPath const& id = prim->GetId();
    SdfPath const& instancerId = prim->GetInstancerId();

    HdBufferSourceSharedPtrVector sources;

    if (HdChangeTracker::IsTransformDirty(*dirtyBits, id)) {
        const GfMatrix4d transform = delegate->GetTransform(id);
        sharedData->bounds.SetMatrix(transform); // for CPU frustum culling

        // doubles should be fine? I think this is just about allocating buffers that
        // our shaders can read. Since the client will decide this, we can use full
        // precision here.
        bool const doublesSupported = true;

        sources.push_back(
            std::make_shared<HdVtBufferSource>(
                HdTokens->transform, transform, doublesSupported));

        sources.push_back(
            std::make_shared<HdVtBufferSource>(
                HdTokens->transformInverse, transform.GetInverse(),
                doublesSupported));

        bool leftHanded = transform.IsLeftHanded();

        // If this is a prototype (has instancer),
        // also push the instancer transform separately.
        if (!instancerId.IsEmpty()) {
            // Gather all instancer transforms in the instancing hierarchy
            const VtMatrix4dArray rootTransforms = 
                prim->GetInstancerTransforms(delegate);
            VtMatrix4dArray rootInverseTransforms(rootTransforms.size());
            for (size_t i = 0; i < rootTransforms.size(); ++i) {
                rootInverseTransforms[i] = rootTransforms[i].GetInverse();
                // Flip the handedness if necessary
                leftHanded ^= rootTransforms[i].IsLeftHanded();
            }

            sources.push_back(
                std::make_shared<HdVtBufferSource>(
                    HdInstancerTokens->instancerTransform,
                    rootTransforms,
                    rootTransforms.size(),
                    doublesSupported));
            sources.push_back(
                std::make_shared<HdVtBufferSource>(
                    HdInstancerTokens->instancerTransformInverse,
                    rootInverseTransforms,
                    rootInverseTransforms.size(),
                    doublesSupported));

            // This seems to just be for shader optimization purposes,
            // we might not need it, but would need to verify that in
            // downstream computations.
            sources.push_back(
                std::make_shared<HdVtBufferSource>(
                    HdTokens->isFlipped,
                    VtValue(int(leftHanded))));
        }

        if (hasMirroredTransform) {
            *hasMirroredTransform = leftHanded;
        }
    }
    if (HdChangeTracker::IsExtentDirty(*dirtyBits, id)) {
        // Note: If the scene description doesn't provide the extents, we use
        // the default constructed GfRange3d which is [FLT_MAX, -FLT_MAX],
        // which disables frustum culling for the prim.
        sharedData->bounds.SetRange(prim->GetExtent(delegate));

        GfVec3d const & localMin = drawItem->GetBounds().GetBox().GetMin();
        HdBufferSourceSharedPtr sourceMin = std::make_shared<HdVtBufferSource>(
                                           HdTokens->bboxLocalMin,
                                           VtValue(GfVec4f(
                                               localMin[0],
                                               localMin[1],
                                               localMin[2],
                                               1.0f)));
        sources.push_back(sourceMin);

        GfVec3d const & localMax = drawItem->GetBounds().GetBox().GetMax();
        HdBufferSourceSharedPtr sourceMax = std::make_shared<HdVtBufferSource>(
                                           HdTokens->bboxLocalMax,
                                           VtValue(GfVec4f(
                                               localMax[0],
                                               localMax[1],
                                               localMax[2],
                                               1.0f)));
        sources.push_back(sourceMax);
    }

    if (HdChangeTracker::IsPrimIdDirty(*dirtyBits, id)) {
        int32_t primId = prim->GetPrimId();
        HdBufferSourceSharedPtr source = std::make_shared<HdVtBufferSource>(
                                           HdTokens->primID,
                                           VtValue(primId));
        sources.push_back(source);
    }

    if (HdChangeTracker::IsAnyPrimvarDirty(*dirtyBits, id)) {
        sources.reserve(sources.size()+constantPrimvars.size());
        for (const HdPrimvarDescriptor& pv: constantPrimvars) {
            if (HdChangeTracker::IsPrimvarDirty(*dirtyBits, id, pv.name)) {
                VtValue value = delegate->Get(id, pv.name);
                // We can exclude any primvar type that we don't support here.
                //if (value.IsHolding<std::string>() ||
                //    value.IsHolding<VtStringArray>() ||
                //    value.IsHolding<TfToken>() ||
                //    value.IsHolding<VtTokenArray>()) {
                //    continue;
                //}

                if (value.IsArrayValued() && value.GetArraySize() == 0) {
                    // A value holding an empty array does not count as an
                    // empty value. Catch that case here.
                    //
                    // Do nothing in this case.
                } else if (!value.IsEmpty()) {
                    // Given that this is a constant primvar, if it is
                    // holding VtArray then use that as a single array
                    // value rather than as one value per element.
                    HdBufferSourceSharedPtr source =
                        std::make_shared<HdVtBufferSource>(pv.name, value,
                            value.IsArrayValued() ? value.GetArraySize() : 1);

                    // Skip buffer source if tuple type is invalid.
                    if (!TF_VERIFY(
                            source->GetTupleType().type != HdTypeInvalid)) {
                        continue;
                    }
                    if (!TF_VERIFY(source->GetTupleType().count > 0)) {
                        continue;
                    }
                 
                    sources.push_back(source);
                }
            }
        }
    }
    
    // Write these computations to the resource registry
    if (!sources.empty()) {
        resourceRegistry->AddPrimvarSources(id, std::move(sources),
                          HdInterpolation::HdInterpolationConstant);
    }
}

} // namespace HydraPassthroughPrimUtil

PXR_NAMESPACE_CLOSE_SCOPE

