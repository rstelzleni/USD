//
// Copyright 2019 Pixar
//
// Licensed under the terms set forth in the LICENSE.txt file available at
// https://openusd.org/license.
//
#include "pxr/imaging/hdSt/volume.h"

#include "pxr/imaging/hdSt/drawItem.h"
#include "pxr/imaging/hdSt/field.h"
#include "pxr/imaging/hdSt/material.h"
#include "pxr/imaging/hdSt/materialNetworkShader.h"
#include "pxr/imaging/hdSt/materialParam.h"
#include "pxr/imaging/hdSt/package.h"
#include "pxr/imaging/hdSt/primUtils.h"
#include "pxr/imaging/hdSt/renderParam.h"
#include "pxr/imaging/hdSt/resourceBinder.h"
#include "pxr/imaging/hdSt/resourceRegistry.h"
#include "pxr/imaging/hdSt/textureBinder.h"
#include "pxr/imaging/hdSt/tokens.h"
#include "pxr/imaging/hdSt/volumeShader.h"
#include "pxr/imaging/hdSt/volumeShaderKey.h"

#include "pxr/imaging/hd/sceneDelegate.h"
#include "pxr/imaging/hd/vtBufferSource.h"

#include "pxr/imaging/hf/diagnostic.h"

#include "pxr/imaging/hio/glslfx.h"

#include "pxr/base/tf/staticTokens.h"

PXR_NAMESPACE_OPEN_SCOPE

TF_DEFINE_PRIVATE_TOKENS(
    _fallbackShaderTokens,

    (density)
    (emission)
);

const float HdStVolume::defaultStepSize                 =   1.0f;
const float HdStVolume::defaultStepSizeLighting         =  10.0f;
const float HdStVolume::defaultMaxTextureMemoryPerField = 128.0f;

HdStVolume::HdStVolume(SdfPath const& id)
    : HdVolume(id)
{
}

HdStVolume::~HdStVolume() = default;

// Dirty bits requiring recomputing the material shader and the
// bounding box.
static const int _shaderAndBBoxComputationDirtyBitsMask =
    HdChangeTracker::Clean 
    | HdChangeTracker::DirtyExtent
    | HdChangeTracker::DirtyMaterialId
    | HdChangeTracker::DirtyRepr
    | HdChangeTracker::DirtyVolumeField;

static const int _initialDirtyBitsMask =
    _shaderAndBBoxComputationDirtyBitsMask
    | HdChangeTracker::DirtyPrimID
    | HdChangeTracker::DirtyPrimvar
    | HdChangeTracker::DirtyTransform
    | HdChangeTracker::DirtyVisibility
    | HdChangeTracker::DirtyInstancer;

HdDirtyBits 
HdStVolume::GetInitialDirtyBitsMask() const
{
    int mask = _initialDirtyBitsMask;
    return (HdDirtyBits)mask;
}

HdDirtyBits 
HdStVolume::_PropagateDirtyBits(HdDirtyBits bits) const
{
    return bits;
}

void 
HdStVolume::_InitRepr(TfToken const &reprToken, HdDirtyBits* dirtyBits)
{
    // All representations point to _volumeRepr.
    if (!_volumeRepr) {
        _volumeRepr = std::make_shared<HdRepr>();
        auto drawItem = std::make_unique<HdStDrawItem>(&_sharedData);
        // Make sure we never replace this material by the default material
        // network (_GetFallbackMaterialNetworkShader in drawBatch.cpp) which
        // simply does not work with the volume render pass shader.
        drawItem->SetMaterialIsFinal(true);
        HdDrawingCoord *drawingCoord = drawItem->GetDrawingCoord();
        // Set up drawing coord instance primvars.
        drawingCoord->SetInstancePrimvarBaseIndex(
            HdStVolume::InstancePrimvar);
        _volumeRepr->AddDrawItem(std::move(drawItem));
        *dirtyBits |= HdChangeTracker::NewRepr;
    }

    _ReprVector::iterator it = std::find_if(_reprs.begin(), _reprs.end(),
                                            _ReprComparator(reprToken));
    bool isNew = it == _reprs.end();
    if (isNew) {
        // add new repr
        it = _reprs.insert(_reprs.end(),
                std::make_pair(reprToken, _volumeRepr));
    }
}

void
HdStVolume::UpdateRenderTag(HdSceneDelegate *delegate,
                            HdRenderParam *renderParam)
{
    HdStUpdateRenderTag(delegate, renderParam, this);
}

void
HdStVolume::Sync(HdSceneDelegate *delegate,
                 HdRenderParam   *renderParam,
                 HdDirtyBits     *dirtyBits,
                 TfToken const   &reprToken)
{
    _UpdateVisibility(delegate, dirtyBits);

    if (*dirtyBits & HdChangeTracker::DirtyMaterialId) {
        HdStSetMaterialId(delegate, renderParam, this);

        HdStDrawItem * const drawItem = static_cast<HdStDrawItem*>(
            _volumeRepr->GetDrawItem(0));
        HdStSetMaterialTag(renderParam, drawItem, 
            HdStMaterialTagTokens->volume);
    }

    _UpdateRepr(delegate, renderParam, reprToken, dirtyBits);

    // This clears all the non-custom dirty bits. This ensures that the rprim
    // doesn't have pending dirty bits that add it to the dirty list every
    // frame.
    // XXX: GetInitialDirtyBitsMask sets certain dirty bits that aren't
    // reset (e.g. DirtyExtent, DirtyPrimID) that make this necessary.
    *dirtyBits &= ~HdChangeTracker::AllSceneDirtyBits;
}

void
HdStVolume::Finalize(HdRenderParam *renderParam)
{
    HdStMarkGarbageCollectionNeeded(renderParam);

    HdStRenderParam * const stRenderParam =
        static_cast<HdStRenderParam*>(renderParam);

    // Decrement material tag count for volume material tag
    stRenderParam->DecreaseMaterialTagCount(HdStMaterialTagTokens->volume);
    stRenderParam->DecreaseRenderTagCount(GetRenderTag());
}

void
HdStVolume::_UpdateRepr(HdSceneDelegate *sceneDelegate,
                        HdRenderParam *renderParam,
                        TfToken const &reprToken,
                        HdDirtyBits *dirtyBits)
{
    HD_TRACE_FUNCTION();
    HF_MALLOC_TAG_FUNCTION();

    HdReprSharedPtr const &curRepr = _volumeRepr;

    if (TfDebug::IsEnabled(HD_RPRIM_UPDATED)) {
        HdChangeTracker::DumpDirtyBits(*dirtyBits);
    }

    HdStDrawItem * const drawItem = static_cast<HdStDrawItem*>(
        curRepr->GetDrawItem(0));

    if (HdChangeTracker::IsDirty(*dirtyBits)) {
        _UpdateDrawItem(sceneDelegate, renderParam, drawItem, dirtyBits);
    }

    *dirtyBits &= ~HdChangeTracker::NewRepr;
}

namespace {

// Fallback volume data created from shaders/fallbackVolume.glslfx
HdStMaterial::VolumeMaterialData
_MakeFallbackVolumeMaterialData()
{
    const HioGlslfx glslfx(HdStPackageFallbackVolumeShader());

    return
        {
            glslfx.GetVolumeSource(),
            {
                HdSt_MaterialParam(
                    HdSt_MaterialParam::ParamTypeFieldRedirect,
                    _fallbackShaderTokens->density,
                    VtValue(0.0f),
                    { _fallbackShaderTokens->density }),
                HdSt_MaterialParam(
                    HdSt_MaterialParam::ParamTypeFieldRedirect,
                    _fallbackShaderTokens->emission,
                    VtValue(GfVec3f(0.0, 0.0, 0.0)),
                    { _fallbackShaderTokens->emission })
            }
        };
}

const
HdStMaterial::VolumeMaterialData &
_ComputeVolumeMaterialData(const HdStMaterial * const material)
{
    // Try to use volume material data from material.
    if (material) {
        const HdStMaterial::VolumeMaterialData &data =
            material->GetVolumeMaterialData();
        if (!data.source.empty()) {
            return data;
        }
    }

    // Instantiate fallback volume shader only once
    //
    // Note that the default HdStMaterial provides a fallback surface
    // shader and we need a volume shader, so we create the shader here
    // ourselves.
    static const HdStMaterial::VolumeMaterialData fallbackData =
        _MakeFallbackVolumeMaterialData();
    return fallbackData;
}

// A map from name to HdStVolumeFieldDescriptor (identifying a
// field prim).
//
// Initialized from a volume prim identified by its path. In the usd world,
// this map is created by following the field:NAME relationships on the volume
// prim to the targeted field prims. The information identifiying the field
// prim is inserted under the key NAME.
//
class _NameToFieldDescriptor
{
public:
    // Get information from scene delegate and create map.
    //
    // Issue validation error if relationship did not target a field prim.
    //
    _NameToFieldDescriptor(
        HdSceneDelegate * const sceneDelegate,
        const SdfPath &id)
      : _descriptors(sceneDelegate->GetVolumeFieldDescriptors(id))
    {
        for (const HdVolumeFieldDescriptor &desc : _descriptors) {
            if (dynamic_cast<HdStField*>(
                    sceneDelegate->GetRenderIndex().GetBprim(
                        desc.fieldPrimType, desc.fieldId))) {

                _nameToDescriptor.insert({desc.fieldName, &desc});

            } else {
                HF_VALIDATION_WARN(
                    id,
                    "Volume has field relationship to non-field prim %s.",
                    desc.fieldId.GetText());
            }
        }
    }

    // Get information identifiying field prim associated to given name.
    //
    // Returns nullptr if no such field prim. Lifetime of returned object
    // is tied to _NameToFieldDescriptor.
    //
    const HdVolumeFieldDescriptor *
    GetDescriptor(const TfToken &name) const {
        const auto it = _nameToDescriptor.find(name);
        if (it == _nameToDescriptor.end()) {
            return nullptr;
        }
        return it->second;
    }

private:
    using _NameToDescriptor = 
        std::unordered_map<TfToken,
                           const HdVolumeFieldDescriptor *,
                           TfToken::HashFunctor>;
    HdVolumeFieldDescriptorVector _descriptors;
    _NameToDescriptor _nameToDescriptor;
};

// Add GLSL code such as "HdGet_density(vec3 p)" for sampling the fields
// to the volume shader code and add necessary 3d textures and other
// parameters and buffer sources to the resulting HdSt_VolumeShader.
// HdMaterialParam's are consulted to figure out the names of the fields
// to sample and the names of the associated sampling functions to generate.
//
// The resulting shader can also fill the points bar of the volume computed
// from the bounding box of the volume.
//
HdSt_VolumeShaderSharedPtr
_ComputeMaterialNetworkShader(
    HdSceneDelegate * const sceneDelegate,
    const SdfPath &id,
    const HdStMaterial::VolumeMaterialData &volumeMaterialData,
    const GfRange3d &authoredExtents)
{
    TRACE_FUNCTION();

    HdStResourceRegistrySharedPtr const resourceRegistry =
        std::static_pointer_cast<HdStResourceRegistry>(
            sceneDelegate->GetRenderIndex().GetResourceRegistry());

    // Generate new shader from volume shader
    HdSt_VolumeShaderSharedPtr const result =
        std::make_shared<HdSt_VolumeShader>();

    // Buffer specs and source for the shader BAR
    HdBufferSpecVector bufferSpecs;
    HdBufferSourceSharedPtrVector bufferSources;

    // The names of the fields read by field readers.
    std::set<TfToken> fieldNames;

    for (const auto & param : volumeMaterialData.params) {
        // Scan original parameters...
        if ( param.IsFieldRedirect() ||
             param.IsPrimvarRedirect() ||
             param.IsFallback() ) {
            // Add fallback values for parameters
            HdSt_MaterialNetworkShader::AddFallbackValueToSpecsAndSources(
                param, &bufferSpecs, &bufferSources);

            if (param.IsFieldRedirect()) {
                // Determine the name of the field the field reader requests.
                TfTokenVector const &names = param.samplerCoords;
                if (!names.empty()) {
                    fieldNames.insert(names[0]);
                }
            }
        }
        // Ignoring 2D texture parameters for volumes.
    }

    // Make a copy of the original params
    HdSt_MaterialParamVector params = volumeMaterialData.params;

    // Note that it is a requirement of HdSt_VolumeShader that
    // namedTextureHandles and fieldDescs line up.
    HdStShaderCode::NamedTextureHandleVector namedTextureHandles;
    HdVolumeFieldDescriptorVector fieldDescs;

    const _NameToFieldDescriptor _nameToFieldDescriptor(sceneDelegate, id);

    // For each requested field name, record the information needed to
    // allocate the necessary texture later:
    // - a texture HdSt_MaterialParam
    // - an HdVolumeFieldDescriptor identifying the HdStField prim holding
    //   the path to the texture
    // - a HdStShader::NamedTextureHandle initialized with a null-handle.
    //
    for (const auto & fieldName : fieldNames) {
        // See whether we have the the field in the volume field
        // descriptors given to us by the scene delegate.
        const HdVolumeFieldDescriptor * const desc =
            _nameToFieldDescriptor.GetDescriptor(fieldName);
        if (!desc) {
            // Invalid field prim, skip.
            continue;
        }
        
        // Record field descriptor
        fieldDescs.push_back(*desc);

        const TfToken textureName(
            fieldName.GetString() +
            HdSt_ResourceBindingSuffixTokens->texture.GetString());
        static const HdStTextureType textureType = HdStTextureType::Field;

        // Produce HdGet_FIELDNAME_texture(vec3 p) to sample
        // the texture.
        const HdSt_MaterialParam param(
            HdSt_MaterialParam::ParamTypeTexture,
            textureName,
            VtValue(GfVec4f(0)),
            TfTokenVector(),
            textureType);

        HdSt_MaterialNetworkShader::AddFallbackValueToSpecsAndSources(
            param, &bufferSpecs, &bufferSources);

        params.push_back(param);

        namedTextureHandles.push_back(
            { textureName, textureType, {}, desc->fieldId.GetHash() });
    }

    result->SetNamedTextureHandles(namedTextureHandles);
    result->SetFieldDescriptors(fieldDescs);
    result->UpdateTextureHandles(sceneDelegate);
    // Get now allocated texture handles
    namedTextureHandles = result->GetNamedTextureHandles();

    const bool doublesSupported = resourceRegistry->GetHgi()->
        GetCapabilities()->IsSet(
            HgiDeviceCapabilitiesBitsShaderDoublePrecision);

    // Get buffer specs for textures (i.e., for
    // field sampling transforms and bindless texture handles).
    HdSt_TextureBinder::GetBufferSpecs(namedTextureHandles, &bufferSpecs,
        doublesSupported);

    // Create params (so that HdGet_... are created) and buffer specs,
    // to communicate volume bounding box and sample distance to shader.
    HdSt_VolumeShader::GetParamsAndBufferSpecsForBBoxAndSampleDistance(
        &params, &bufferSpecs, doublesSupported);

    const bool hasField = !namedTextureHandles.empty();

    // If there is a field, we postpone giving buffer sources for
    // the volume bounding box until after the textures have been
    // committed.
    if (!hasField) {
        HdSt_VolumeShader::GetBufferSourcesForBBoxAndSampleDistance(
            { GfBBox3d(authoredExtents), 1.0f },
            &bufferSources,
            doublesSupported);
    }

    // Make volume shader responsible if we have fields with bounding
    // boxes.
    result->SetFillsPointsBar(hasField);
    result->SetParams(params);
    result->SetBufferSources(
        bufferSpecs, std::move(bufferSources), resourceRegistry);
    
    // Append the volume shader (calling into the GLSL functions
    // generated above)
    result->SetFragmentSource(volumeMaterialData.source);

    return result;
}

VtValue
_ComputeBBoxVertices(GfRange3d const &range)
{
    VtVec3fArray result(8);

    const GfVec3d min = HdSt_VolumeShader::GetSafeMin(range);
    const GfVec3d max = HdSt_VolumeShader::GetSafeMax(range);

    int i = 0;

    for (const double x : { min[0], max[0] }) {
        for (const double y : { min[1], max[1] }) {
            for (const double z : { min[2], max[2] }) {
                result[i] = GfVec3f(x,y,z);
                i++;
            }
        }
    }

    return VtValue(result);
}

const VtValue &
_GetCubeTriangleIndices()
{
    static const VtValue result(
        VtVec3iArray{
                GfVec3i(1,3,2),
                GfVec3i(0,1,2),

                GfVec3i(7,5,4),
                GfVec3i(6,7,4),

                GfVec3i(5,1,0),
                GfVec3i(4,5,0),

                GfVec3i(3,7,6),
                GfVec3i(2,3,6),

                GfVec3i(2,6,4),
                GfVec3i(0,2,4),

                GfVec3i(7,3,1),
                GfVec3i(5,7,1)});

    return result;
}

} // end namespace

void
HdStVolume::_UpdateDrawItem(HdSceneDelegate *sceneDelegate,
                            HdRenderParam *renderParam,
                            HdStDrawItem *drawItem,
                            HdDirtyBits *dirtyBits)
{
    HD_TRACE_FUNCTION();
    HF_MALLOC_TAG_FUNCTION();


    /* INSTANCE PRIMVARS */
    _UpdateInstancer(sceneDelegate, dirtyBits);
    HdStUpdateInstancerData(sceneDelegate->GetRenderIndex(),
                            renderParam,
                            this,
                            drawItem,
                            &_sharedData,
                            *dirtyBits);

    if (HdStShouldPopulateConstantPrimvars(dirtyBits, GetId())) {
        /* CONSTANT PRIMVARS, TRANSFORM AND EXTENT */
        const HdPrimvarDescriptorVector constantPrimvars =
            HdStGetPrimvarDescriptors(this, drawItem, sceneDelegate,
                                      HdInterpolationConstant);
        HdStPopulateConstantPrimvars(this,
                                     &_sharedData,
                                     sceneDelegate,
                                     renderParam,
                                     drawItem,
                                     dirtyBits,
                                     constantPrimvars);
    }

    bool updatedTextureHandles = false;
    if ((*dirtyBits) & HdChangeTracker::DirtyMaterialId) {
        /* MATERIAL SHADER (may affect subsequent primvar population) */

        // Note that the creation of the HdSt_VolumeShader and the
        // allocation of the necessary textures is driven by two
        // different dirtyBits (HdChangeTracker::DirtyMaterialId and
        // HdChangeTracker::DirtyVolumeField).
        //
        // This way, we do not need to re-create the shader on every frame
        // when the fields of a volume are animated.
        //
        const HdStMaterial * const material = static_cast<const HdStMaterial *>(
            sceneDelegate->GetRenderIndex().GetSprim(
                HdPrimTypeTokens->material, GetMaterialId()));

        // Compute the material shader by adding GLSL code such as
        // "HdGet_density(vec3 p)" for sampling the fields needed by the volume
        // shader.
        // The material shader will eventually be concatenated with
        // the geometry shader which does the raymarching and is calling into
        // GLSL functions such as "float scattering(vec3)" in the volume shader
        // to evaluate physical properties of a volume at the point p.
        
        drawItem->SetMaterialNetworkShader(
            _ComputeMaterialNetworkShader(
                sceneDelegate,
                GetId(),
                _ComputeVolumeMaterialData(material),
                _sharedData.bounds.GetRange()));
        updatedTextureHandles = true;
    }        

    HdStResourceRegistrySharedPtr resourceRegistry =
        std::static_pointer_cast<HdStResourceRegistry>(
            sceneDelegate->GetRenderIndex().GetResourceRegistry());

    HdSt_VolumeShaderSharedPtr const materialNetworkShader =
        std::dynamic_pointer_cast<HdSt_VolumeShader>(
            drawItem->GetMaterialNetworkShader());

    if (!materialNetworkShader) {
        TF_CODING_ERROR("Expected valid volume shader for draw item.");
        return;
    }

    // We do not need to call UpdateTextureHandles() on the 
    // materialNetworkShader if DirtyMaterialId, as it was already called
    // during _ComputeMaterialNetworkShader().
    if (((*dirtyBits) & (HdChangeTracker::DirtyVolumeField)) && 
        !updatedTextureHandles) {
        /* FIELD TEXTURES */
        
        // (Re-)Allocate the textures associated with the field prims.
        materialNetworkShader->UpdateTextureHandles(sceneDelegate);
    }

    /* VERTICES */
    if ((*dirtyBits) & _shaderAndBBoxComputationDirtyBitsMask) {
        // Any change to the bounding box requires us to recompute
        // the vertices
        //
        if (!HdStIsValidBAR(drawItem->GetVertexPrimvarRange())) {
            static const HdBufferSpecVector bufferSpecs{
                HdBufferSpec(HdTokens->points,
                             HdTupleType{ HdTypeFloatVec3, 1 })
            };

            HdBufferArrayRangeSharedPtr const range =
                resourceRegistry->AllocateNonUniformBufferArrayRange(
                    HdTokens->primvar, bufferSpecs,
                    HdBufferArrayUsageHintBitsVertex);
            _sharedData.barContainer.Set(
                drawItem->GetDrawingCoord()->GetVertexPrimvarIndex(), range);
        }

        // Let HdSt_VolumeShader know about the points bar so that it
        // can fill it with the vertices of the volume bounding box.
        materialNetworkShader->SetPointsBar(drawItem->GetVertexPrimvarRange());
        
        // If HdSt_VolumeShader is not in charge of filling the points bar
        // from the volume bounding box computed from the fields, ...
        if (!materialNetworkShader->GetFillsPointsBar()) {
            // ... fill the points from the authored extents.
            resourceRegistry->AddSource(
                drawItem->GetVertexPrimvarRange(),
                std::make_shared<HdVtBufferSource>(
                    HdTokens->points,
                    _ComputeBBoxVertices(
                        _sharedData.bounds.GetRange())));
        }
    }

    if ((*dirtyBits) & HdChangeTracker::NewRepr) {
        // Bounding box topology and geometric shader key only need to
        // be initialized the first time we make the draw item.

        const HdSt_VolumeShaderKey shaderKey;
        drawItem->SetGeometricShader(
            HdSt_GeometricShader::Create(shaderKey, resourceRegistry));

        /* TRIANGLE INDICES */
        {
            // XXX:
            // Always the same triangle indices, should they be allocated only
            // once and shared across all volumes?
            HdBufferSourceSharedPtr const source =
                std::make_shared<HdVtBufferSource>(
                    HdTokens->indices, _GetCubeTriangleIndices());
            
            HdBufferSourceSharedPtrVector sources = { source };
            
            if (!HdStIsValidBAR(drawItem->GetTopologyRange())) {
                HdBufferSpecVector bufferSpecs;
                HdBufferSpec::GetBufferSpecs(sources, &bufferSpecs);
                
                HdBufferArrayRangeSharedPtr const range =
                    resourceRegistry->AllocateNonUniformBufferArrayRange(
                        HdTokens->primvar, bufferSpecs,
                        HdBufferArrayUsageHintBitsIndex);
                _sharedData.barContainer.Set(
                    drawItem->GetDrawingCoord()->GetTopologyIndex(), range);
            }
            
            resourceRegistry->AddSources(drawItem->GetTopologyRange(),
                                         std::move(sources));
        }
    }
}

PXR_NAMESPACE_CLOSE_SCOPE
