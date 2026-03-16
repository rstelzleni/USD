#include "primUtil.h"

#include "pxr/usdImaging/hydraPassthrough/constantVtBufferSource.h"
#include "pxr/usdImaging/hydraPassthrough/resourceRegistry.h"
#include "pxr/imaging/hd/bufferSource.h"
#include "pxr/imaging/hd/vtBufferSource.h"

#include "pxr/usd/sdf/assetPath.h"
#include "pxr/usd/sdf/pathExpression.h"

#include "pxr/base/gf/matrix2d.h"
#include "pxr/base/gf/matrix2f.h"

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

        // We wrap transform in a VtValue so that the HdVtBufferSource doesn't
        // downcast the matrix to float if the default matrix type is float.
        // There's no reason not to preserve full precision here.
        //
        // Note we can also prevent the downcast with the environment variable
        // HD_ENABLE_DOUBLEMATRIX, but in our case this is not env specific
        sources.push_back(
            std::make_shared<HdVtBufferSource>(
                HdTokens->transform, VtValue(transform), doublesSupported));

        sources.push_back(
            std::make_shared<HdVtBufferSource>(
                HdTokens->transformInverse, VtValue(transform.GetInverse()),
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
                // We have to use a special case buffer source type to preserve
                // these types. The HdVtBufferSource can store them, but it
                // will return nullptr when you try to access the stored data
                // because it only supports a hard coded list of data types.
                //
                // We can use this local type to special case these, but this
                // won't work for downstream computations that want to operate
                // on a void* with a fixed element size, like subdivision. This
                // probably requires more thought, for instance, do we need
                // subdivision for these if they use non-constant
                // interpolation? Figure that out before using this special
                // casing for the other primvar interpolation types.
                //
                // Note that HdSt only excludes the string and token types
                // here, but in testing it seems we also need to skip int64,
                // uint64, and matrix2 types. You can tell what types need to
                // be skipped by checking the conversion code in HdGetValueData
                // (hd/types.cpp)
                if (value.IsHolding<std::string>() ||
                    value.IsHolding<VtStringArray>() ||
                    value.IsHolding<TfToken>() ||
                    value.IsHolding<VtTokenArray>() ||
                    value.IsHolding<SdfAssetPath>() ||
                    value.IsHolding<VtArray<SdfAssetPath>>() ||
                    value.IsHolding<SdfPathExpression>() ||
                    value.IsHolding<int64_t>() ||
                    value.IsHolding<VtArray<int64_t>>() ||
                    value.IsHolding<uint64_t>() ||
                    value.IsHolding<VtArray<uint64_t>>() ||
                    value.IsHolding<GfMatrix2f>() ||
                    value.IsHolding<VtArray<GfMatrix2f>>() ||
                    value.IsHolding<GfMatrix2d>() ||
                    value.IsHolding<VtArray<GfMatrix2d>>()) {
                    sources.push_back(
                        std::make_shared<HydraPassthroughConstantVtBufferSource>(
                            pv.name, value,
                            value.IsArrayValued() ? value.GetArraySize() : 1));
                    continue;
                }

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

                    if (source->GetTupleType().type == HdTypeInvalid) {
                        TF_WARN("Unsupported primvar type %s for primvar %s prim %s. Skipping.",
                                value.GetTypeName().c_str(),
                                pv.name.GetText(), id.GetText());
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

