//
// Copyright 2017 Pixar
//
// Licensed under the terms set forth in the LICENSE.txt file available at
// https://openusd.org/license.
//
#include "pxr/pxr.h"
#include "pxr/imaging/hdSt/renderDelegate.h"

#include "pxr/imaging/hdSt/basisCurves.h"
#include "pxr/imaging/hdSt/drawItemsCache.h"
#include "pxr/imaging/hdSt/drawTarget.h"
#include "pxr/imaging/hdSt/extComputation.h"
#include "pxr/imaging/hdSt/field.h"
#include "pxr/imaging/hdSt/glslfxShader.h"
#include "pxr/imaging/hdSt/instancer.h"
#include "pxr/imaging/hdSt/light.h"
#include "pxr/imaging/hdSt/material.h"
#include "pxr/imaging/hdSt/mesh.h"
#include "pxr/imaging/hdSt/package.h"
#include "pxr/imaging/hdSt/points.h"
#include "pxr/imaging/hdSt/renderBuffer.h"
#include "pxr/imaging/hdSt/renderPass.h"
#include "pxr/imaging/hdSt/renderPassState.h"
#include "pxr/imaging/hdSt/renderParam.h"
#include "pxr/imaging/hdSt/tokens.h"
#include "pxr/imaging/hdSt/resourceRegistry.h"
#include "pxr/imaging/hdSt/volume.h"

#include "pxr/imaging/hd/aov.h"
#include "pxr/imaging/hd/camera.h"
#include "pxr/imaging/hd/driver.h"
#include "pxr/imaging/hd/extComputation.h"
#include "pxr/imaging/hd/imageShader.h"
#include "pxr/imaging/hd/perfLog.h"
#include "pxr/imaging/hd/tokens.h"

#include "pxr/imaging/hgi/hgi.h"
#include "pxr/imaging/hgi/tokens.h"

#include "pxr/imaging/glf/diagnostic.h"
#include "pxr/imaging/hio/glslfx.h"

#include "pxr/base/tf/envSetting.h"
#include "pxr/base/tf/getenv.h"
#include "pxr/base/tf/staticTokens.h"


PXR_NAMESPACE_OPEN_SCOPE

TF_DEFINE_ENV_SETTING(HD_ENABLE_GPU_TINY_PRIM_CULLING, false,
                      "Enable tiny prim culling");

TF_DEFINE_ENV_SETTING(HDST_MAX_LIGHTS, 16,
                      "Maximum number of lights to render with");

const TfTokenVector HdStRenderDelegate::SUPPORTED_RPRIM_TYPES =
{
    HdPrimTypeTokens->mesh,
    HdPrimTypeTokens->basisCurves,
    HdPrimTypeTokens->points,
    HdPrimTypeTokens->volume
};

const TfTokenVector HdStRenderDelegate::SUPPORTED_SPRIM_TYPES =
{
    HdPrimTypeTokens->camera,
    HdPrimTypeTokens->drawTarget,
    HdPrimTypeTokens->extComputation,
    HdPrimTypeTokens->material,
    HdPrimTypeTokens->domeLight,
    HdPrimTypeTokens->cylinderLight,
    HdPrimTypeTokens->diskLight,
    HdPrimTypeTokens->distantLight,
    HdPrimTypeTokens->rectLight,
    HdPrimTypeTokens->simpleLight,
    HdPrimTypeTokens->sphereLight,
    HdPrimTypeTokens->imageShader
};

#ifdef PXR_MATERIALX_SUPPORT_ENABLED
TF_DEFINE_PRIVATE_TOKENS(
    _tokens,
    (mtlx)
);
#endif

using HdStResourceRegistryWeakPtr =  std::weak_ptr<HdStResourceRegistry>;

namespace {

//
// Map from Hgi instances to resource registries.
//
// An entry is kept alive until the last shared_ptr to a resource
// registry is dropped.
//
class _HgiToResourceRegistryMap final :
    public std::enable_shared_from_this<_HgiToResourceRegistryMap>
{
public:
    // Map is a singleton.
    static _HgiToResourceRegistryMap &GetInstance()
    {
        static auto instance = std::make_shared<_HgiToResourceRegistryMap>();
        return *instance;
    }

    // Look-up resource registry by Hgi instance, create resource
    // registry for the instance if it didn't exist.
    HdStResourceRegistrySharedPtr GetOrCreateRegistry(Hgi * const hgi)
    {
        std::lock_guard<std::mutex> guard(_mutex);

        // Previous entry exists, use it.
        HdStResourceRegistryWeakPtr &registry = _map[hgi];
        if (HdStResourceRegistrySharedPtr const result = registry.lock()) {
            return result;
        }

        // Create resource registry, custom deleter to remove corresponding
        // entry from map.
        HdStResourceRegistrySharedPtr result{
            new HdStResourceRegistry(hgi),
            [maybeSelf = weak_from_this()](
                HdStResourceRegistry * const registry) {
                // If a resource registry has a static lifetime object as its
                // root owner, then we can encounter a static lifetime ordering
                // issue, since this registry map also has a static lifetime.
                // It's possible that due to the unspecified ordering of static
                // destruction, this map is destroyed before a registry, in
                // which case calling _Unregister() would be undefined
                // behaviour. To prevent this we use a weak ownership, and skip
                // the call when the map is dead because it no longer matters.
                if (const auto self = maybeSelf.lock()) {
                    self->_Unregister(registry);
                }
                delete registry;
            }
        };

        // Insert into map.
        registry = result;

        // Also register with HdPerfLog.
        HdPerfLog::GetInstance().AddResourceRegistry(result.get());

        return result;
    }

private:
    void _Unregister(HdStResourceRegistry * const registry)
    {
        TRACE_FUNCTION();

        std::lock_guard<std::mutex> guard(_mutex);

        HdPerfLog::GetInstance().RemoveResourceRegistry(registry);
        
        _map.erase(registry->GetHgi());
    }

    using _Map = std::unordered_map<Hgi*, HdStResourceRegistryWeakPtr>;

    std::mutex _mutex;
    _Map _map;
};

}

HdStRenderDelegate::HdStRenderDelegate()
    : HdStRenderDelegate(HdRenderSettingsMap())
{
}

HdStRenderDelegate::HdStRenderDelegate(HdRenderSettingsMap const& settingsMap)
    : HdRenderDelegate(settingsMap)
    , _hgi(nullptr)
    , _renderParam(std::make_unique<HdStRenderParam>())
    , _drawItemsCache(std::make_unique<HdSt_DrawItemsCache>())
{
    // Initialize the settings and settings descriptors.
    _settingDescriptors = {
        HdRenderSettingDescriptor{
            "Enable Tiny Prim Culling",
            HdStRenderSettingsTokens->enableTinyPrimCulling,
            VtValue(bool(TfGetEnvSetting(HD_ENABLE_GPU_TINY_PRIM_CULLING))) },
        HdRenderSettingDescriptor{
            "Step size when raymarching volume",
            HdStRenderSettingsTokens->volumeRaymarchingStepSize,
            VtValue(HdStVolume::defaultStepSize) },
        HdRenderSettingDescriptor{
            "Step size when raymarching volume for lighting computation",
            HdStRenderSettingsTokens->volumeRaymarchingStepSizeLighting,
            VtValue(HdStVolume::defaultStepSizeLighting) },
        HdRenderSettingDescriptor{
            "Maximum memory for a volume field texture in Mb "
            "(unless overridden by field prim)",
            HdStRenderSettingsTokens->volumeMaxTextureMemoryPerField,
            VtValue(HdStVolume::defaultMaxTextureMemoryPerField) },
        HdRenderSettingDescriptor{
            "Maximum number of lights",
            HdStRenderSettingsTokens->maxLights,
            VtValue(int(TfGetEnvSetting(HDST_MAX_LIGHTS))) },
        HdRenderSettingDescriptor{
            "Dome light camera visibility",
            HdRenderSettingsTokens->domeLightCameraVisibility,
            VtValue(true) }
    };

    _PopulateDefaultSettings(_settingDescriptors);
}

HdRenderSettingDescriptorList
HdStRenderDelegate::GetRenderSettingDescriptors() const
{
    return _settingDescriptors;
}

VtDictionary 
HdStRenderDelegate::GetRenderStats() const
{
    VtDictionary ra = _resourceRegistry->GetResourceAllocation();

    const VtDictionary::iterator gpuMemIt = 
        ra.find(HdPerfTokens->gpuMemoryUsed.GetString());
    if (gpuMemIt != ra.end()) {
        // If we find gpuMemoryUsed, add the texture memory to it.
        // XXX: We should look into fixing this in the resource registry itself
        size_t texMem = 
            VtDictionaryGet<size_t>(ra, HdPerfTokens->textureMemory.GetString(),
                VtDefault = 0);
        size_t gpuMemTotal = gpuMemIt->second.Get<size_t>();
        gpuMemIt->second = VtValue(gpuMemTotal + texMem);
    }

    return ra;
}

HdStRenderDelegate::~HdStRenderDelegate() = default;

void
HdStRenderDelegate::SetDrivers(HdDriverVector const& drivers)
{
    if (_resourceRegistry) {
        TF_CODING_ERROR("Cannot set HdDriver twice for a render delegate.");
        return;
    }

    // For Storm we want to use the Hgi driver, so extract it.
    for (HdDriver* hdDriver : drivers) {
        if (hdDriver->name == HgiTokens->renderDriver &&
            hdDriver->driver.IsHolding<Hgi*>()) {
            _hgi = hdDriver->driver.UncheckedGet<Hgi*>();
            break;
        }
    }
    
    TF_VERIFY(_hgi, "HdSt requires Hgi HdDriver");

    _resourceRegistry =
        _HgiToResourceRegistryMap::GetInstance().GetOrCreateRegistry(_hgi);
}

const TfTokenVector &
HdStRenderDelegate::GetSupportedRprimTypes() const
{
    return SUPPORTED_RPRIM_TYPES;
}

const TfTokenVector &
HdStRenderDelegate::GetSupportedSprimTypes() const
{
    return SUPPORTED_SPRIM_TYPES;
}

static
TfTokenVector
_ComputeSupportedBprimTypes()
{
    TfTokenVector result;
    result.push_back(HdPrimTypeTokens->renderBuffer);

    for (const TfToken &primType : HdStField::GetSupportedBprimTypes()) {
        result.push_back(primType);
    }

    return result;
}

const TfTokenVector &
HdStRenderDelegate::GetSupportedBprimTypes() const
{
    static const TfTokenVector result = _ComputeSupportedBprimTypes();
    return result;
}

HdRenderParam *
HdStRenderDelegate::GetRenderParam() const
{
    return _renderParam.get();
}

HdResourceRegistrySharedPtr
HdStRenderDelegate::GetResourceRegistry() const
{
    return _resourceRegistry;
}

static
bool
_AovHasIdSemantic(TfToken const & name)
{
    return name == HdAovTokens->primId ||
           name == HdAovTokens->instanceId ||
           name == HdAovTokens->elementId ||
           name == HdAovTokens->edgeId ||
           name == HdAovTokens->pointId;
}

HdAovDescriptor
HdStRenderDelegate::GetDefaultAovDescriptor(TfToken const& name) const
{
    const bool colorDepthMSAA = true; // GL requires color/depth to be matching.

    if (name == HdAovTokens->color) {
        return HdAovDescriptor(
                HdFormatFloat16Vec4, colorDepthMSAA, VtValue(GfVec4f(0)));
    } else if (HdAovHasDepthStencilSemantic(name)) {
        return HdAovDescriptor(
            HdFormatFloat32UInt8, colorDepthMSAA,
            VtValue(HdDepthStencilType(1.0f, 0)));
    } else if (HdAovHasDepthSemantic(name)) {
        return HdAovDescriptor(
                HdFormatFloat32, colorDepthMSAA, VtValue(1.0f));
    } else if (_AovHasIdSemantic(name)) {
        return HdAovDescriptor(
                HdFormatInt32, colorDepthMSAA, VtValue(-1));
    } else if (name == HdAovTokens->Neye) {
        return HdAovDescriptor(
                HdFormatUNorm8Vec4, colorDepthMSAA, VtValue(GfVec4f(0)));
    }

    return HdAovDescriptor();
}

HdRenderPassSharedPtr
HdStRenderDelegate::CreateRenderPass(HdRenderIndex *index,
                        HdRprimCollection const& collection)
{
    return std::make_shared<HdSt_RenderPass>(index, collection);
}

HdRenderPassStateSharedPtr
HdStRenderDelegate::CreateRenderPassState() const
{
    return std::make_shared<HdStRenderPassState>();
}

HdInstancer *
HdStRenderDelegate::CreateInstancer(HdSceneDelegate *delegate,
                                    SdfPath const& id)
{
    return new HdStInstancer(delegate, id);
}

void
HdStRenderDelegate::DestroyInstancer(HdInstancer *instancer)
{
    delete instancer;
}

HdRprim *
HdStRenderDelegate::CreateRprim(TfToken const& typeId,
                                SdfPath const& rprimId)
{
    if (typeId == HdPrimTypeTokens->mesh) {
        return new HdStMesh(rprimId);
    } else if (typeId == HdPrimTypeTokens->basisCurves) {
        return new HdStBasisCurves(rprimId);
    } else  if (typeId == HdPrimTypeTokens->points) {
        return new HdStPoints(rprimId);
    } else  if (typeId == HdPrimTypeTokens->volume) {
        return new HdStVolume(rprimId);
    } else {
        TF_CODING_ERROR("Unknown Rprim Type %s", typeId.GetText());
    }

    return nullptr;
}

void
HdStRenderDelegate::DestroyRprim(HdRprim *rPrim)
{
    delete rPrim;
}

HdSprim *
HdStRenderDelegate::CreateSprim(TfToken const& typeId,
                                SdfPath const& sprimId)
{
    if (typeId == HdPrimTypeTokens->camera) {
        return new HdCamera(sprimId);
    } else  if (typeId == HdPrimTypeTokens->drawTarget) {
        return new HdStDrawTarget(sprimId);
    } else  if (typeId == HdPrimTypeTokens->extComputation) {
        return new HdStExtComputation(sprimId);
    } else  if (typeId == HdPrimTypeTokens->material) {
        return new HdStMaterial(sprimId);
    } else if (typeId == HdPrimTypeTokens->domeLight ||
                typeId == HdPrimTypeTokens->simpleLight ||
                typeId == HdPrimTypeTokens->sphereLight ||
                typeId == HdPrimTypeTokens->diskLight ||
                typeId == HdPrimTypeTokens->distantLight ||
                typeId == HdPrimTypeTokens->cylinderLight ||
                typeId == HdPrimTypeTokens->rectLight) {
        return new HdStLight(sprimId, typeId);
    } else if (typeId == HdPrimTypeTokens->imageShader) {
        return new HdImageShader(sprimId);
    } else {
        TF_CODING_ERROR("Unknown Sprim Type %s", typeId.GetText());
    }

    return nullptr;
}

HdSprim *
HdStRenderDelegate::CreateFallbackSprim(TfToken const& typeId)
{
    if (typeId == HdPrimTypeTokens->camera) {
        return new HdCamera(SdfPath::EmptyPath());
    } else  if (typeId == HdPrimTypeTokens->drawTarget) {
        return new HdStDrawTarget(SdfPath::EmptyPath());
    } else  if (typeId == HdPrimTypeTokens->extComputation) {
        return new HdStExtComputation(SdfPath::EmptyPath());
    } else  if (typeId == HdPrimTypeTokens->material) {
        return _CreateFallbackMaterialPrim();
    } else if (typeId == HdPrimTypeTokens->domeLight ||
                typeId == HdPrimTypeTokens->simpleLight ||
                typeId == HdPrimTypeTokens->sphereLight ||
                typeId == HdPrimTypeTokens->diskLight ||
                typeId == HdPrimTypeTokens->distantLight ||
                typeId == HdPrimTypeTokens->cylinderLight ||
                typeId == HdPrimTypeTokens->rectLight) {
        return new HdStLight(SdfPath::EmptyPath(), typeId);
    } else if (typeId == HdPrimTypeTokens->imageShader) {
        return new HdImageShader(SdfPath::EmptyPath());
    } else {
        TF_CODING_ERROR("Unknown Sprim Type %s", typeId.GetText());
    }

    return nullptr;
}

void
HdStRenderDelegate::DestroySprim(HdSprim *sPrim)
{
    delete sPrim;
}

HdBprim *
HdStRenderDelegate::CreateBprim(TfToken const& typeId,
                                SdfPath const& bprimId)
{
    if (HdStField::IsSupportedBprimType(typeId)) {
        return new HdStField(bprimId, typeId);
    } else if (typeId == HdPrimTypeTokens->renderBuffer) {
        return new HdStRenderBuffer(_resourceRegistry.get(), bprimId);
    } else {
        TF_CODING_ERROR("Unknown Bprim Type %s", typeId.GetText());
    }

    return nullptr;
}

HdBprim *
HdStRenderDelegate::CreateFallbackBprim(TfToken const& typeId)
{
    if (HdStField::IsSupportedBprimType(typeId)) {
        return new HdStField(SdfPath::EmptyPath(), typeId);
    } else if (typeId == HdPrimTypeTokens->renderBuffer) {
        return new HdStRenderBuffer(_resourceRegistry.get(),
                                    SdfPath::EmptyPath());
    } else {
        TF_CODING_ERROR("Unknown Bprim Type %s", typeId.GetText());
    }

    return nullptr;
}

void
HdStRenderDelegate::DestroyBprim(HdBprim *bPrim)
{
    delete bPrim;
}

HdSprim *
HdStRenderDelegate::_CreateFallbackMaterialPrim()
{
    HioGlslfxSharedPtr glslfx =
        std::make_shared<HioGlslfx>(HdStPackageFallbackMaterialNetworkShader());

    HdSt_MaterialNetworkShaderSharedPtr fallbackShaderCode =
        std::make_shared<HdStGLSLFXShader>(glslfx);

    HdStMaterial *material = new HdStMaterial(SdfPath::EmptyPath());
    material->SetMaterialNetworkShader(fallbackShaderCode);

    return material;
}

void
HdStRenderDelegate::CommitResources(HdChangeTracker *tracker)
{
    TF_UNUSED(tracker);
    GLF_GROUP_FUNCTION();
    
    _ApplyTextureSettings();

    // --------------------------------------------------------------------- //
    // RESOLVE, COMPUTE & COMMIT PHASE
    // --------------------------------------------------------------------- //
    // All the required input data is now resident in memory, next we must:
    //
    //     1) Execute compute as needed for normals, tessellation, etc.
    //     2) Commit resources to the GPU.
    //     3) Update any scene-level acceleration structures.

    // Commit all pending source data.
    _resourceRegistry->Commit();

    HdStRenderParam *stRenderParam = _renderParam.get();
    if (stRenderParam->IsGarbageCollectionNeeded()) {
        _resourceRegistry->GarbageCollect();
        stRenderParam->ClearGarbageCollectionNeeded();
    }

    // see bug126621. currently dispatch buffers need to be released
    //                more frequently than we expect.
    _resourceRegistry->GarbageCollectDispatchBuffers();

    _drawItemsCache->GarbageCollect();
}

bool
HdStRenderDelegate::IsSupported()
{
    return Hgi::IsSupported();
}

TfTokenVector
HdStRenderDelegate::GetShaderSourceTypes() const
{
#ifdef PXR_MATERIALX_SUPPORT_ENABLED
    return {HioGlslfxTokens->glslfx, _tokens->mtlx};
#else
    return {HioGlslfxTokens->glslfx};
#endif
}

TfTokenVector
HdStRenderDelegate::GetMaterialRenderContexts() const
{
#ifdef PXR_MATERIALX_SUPPORT_ENABLED
    return {HioGlslfxTokens->glslfx, _tokens->mtlx};
#else
    return {HioGlslfxTokens->glslfx};
#endif
}

bool
HdStRenderDelegate::IsPrimvarFilteringNeeded() const
{
    return true;
}

HdStDrawItemsCachePtr
HdStRenderDelegate::GetDrawItemsCache() const
{
    return _drawItemsCache.get();
}

Hgi*
HdStRenderDelegate::GetHgi()
{
    return _hgi;
}

void
HdStRenderDelegate::_ApplyTextureSettings()
{
    const float memInMb =
        std::max(0.0f,
                 GetRenderSetting<float>(
                     HdStRenderSettingsTokens->volumeMaxTextureMemoryPerField,
                     HdStVolume::defaultMaxTextureMemoryPerField));

    _resourceRegistry->SetMemoryRequestForTextureType(
        HdStTextureType::Field, 1048576 * memInMb);
}

PXR_NAMESPACE_CLOSE_SCOPE
