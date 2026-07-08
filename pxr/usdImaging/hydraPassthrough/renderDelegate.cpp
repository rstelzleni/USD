#include "renderDelegate.h"
#include "instancer.h"
#include "material.h"
#include "mesh.h"
#include "renderPass.h"
#include "renderParam.h"
#include "resourceRegistry.h"

#include "pxr/base/tf/diagnosticLite.h"
#include "pxr/imaging/hd/camera.h"
#include "pxr/imaging/hd/extComputation.h"

PXR_NAMESPACE_OPEN_SCOPE

const TfTokenVector HydraPassthroughRenderDelegate::SUPPORTED_RPRIM_TYPES = {
    HdPrimTypeTokens->mesh,
};

const TfTokenVector HydraPassthroughRenderDelegate::SUPPORTED_SPRIM_TYPES = {
    HdPrimTypeTokens->camera,
    HdPrimTypeTokens->material,
    HdPrimTypeTokens->extComputation,
};

const TfTokenVector HydraPassthroughRenderDelegate::SUPPORTED_BPRIM_TYPES = {};

HydraPassthroughRenderDelegate::HydraPassthroughRenderDelegate() : HdRenderDelegate() {
    _Initialize();
}

HydraPassthroughRenderDelegate::HydraPassthroughRenderDelegate(
        HdRenderSettingsMap const &settingsMap)
    : HdRenderDelegate(settingsMap) {
        _Initialize();
    }

void HydraPassthroughRenderDelegate::_Initialize() {
    TF_STATUS("Initializing Passthrough RenderDelegate %p", this);
    _resourceRegistry = std::make_shared<HydraPassthroughResourceRegistry>();
    _renderData = HydraPassthroughRenderData::New();
    _renderParam = std::make_unique<HydraPassthroughRenderParam>(_renderData);
    std::static_pointer_cast<HydraPassthroughResourceRegistry>(_resourceRegistry)->SetRenderData(_renderData);
}

HydraPassthroughRenderDelegate::~HydraPassthroughRenderDelegate() {
    _resourceRegistry.reset();
    TF_STATUS("Destroyed Passthrough RenderDelegate %p", this);
}

TfTokenVector const &HydraPassthroughRenderDelegate::GetSupportedRprimTypes() const {
    return SUPPORTED_RPRIM_TYPES;
}

TfTokenVector const &HydraPassthroughRenderDelegate::GetSupportedSprimTypes() const {
    return SUPPORTED_SPRIM_TYPES;
}

TfTokenVector const &HydraPassthroughRenderDelegate::GetSupportedBprimTypes() const {
    return SUPPORTED_BPRIM_TYPES;
}

HdResourceRegistrySharedPtr HydraPassthroughRenderDelegate::GetResourceRegistry() const {
    return _resourceRegistry;
}

void HydraPassthroughRenderDelegate::CommitResources(HdChangeTracker *tracker) {
    
    // Commit resources, which causes the computations to be run.
    _resourceRegistry->Commit();

    for (const auto& it : _cameraMap) {
        HdCamera *cam = it.second;
        // Don't include the internal camera created by our render task.
        if (cam->GetId().GetString().find("/RenderTask/") != std::string::npos) {
            continue;
        }
        _renderData->AddCamera(cam);
    }
}

HdRenderPassSharedPtr
HydraPassthroughRenderDelegate::CreateRenderPass(HdRenderIndex *index,
        HdRprimCollection const &collection) {
    return HdRenderPassSharedPtr(new HydraPassthroughRenderPass(index, collection, this));
}

HdRprim *HydraPassthroughRenderDelegate::CreateRprim(TfToken const &typeId,
        SdfPath const &rprimId) {
    if (typeId == HdPrimTypeTokens->mesh) {
        return new HydraPassthroughMesh(rprimId);
    } else {
        TF_CODING_ERROR("Unknown Rprim type=%s id=%s", typeId.GetText(),
                rprimId.GetText());
    }
    return nullptr;
}

void HydraPassthroughRenderDelegate::DestroyRprim(HdRprim *rPrim) {
    delete rPrim;
}

HdSprim *HydraPassthroughRenderDelegate::CreateSprim(TfToken const &typeId,
        SdfPath const &sprimId) {
    if (typeId == HdPrimTypeTokens->camera) {
        // Track the camera we return here so we can extract the camera data
        // once it's synced.
        HdCamera *camera = new HdCamera(sprimId);
        _cameraMap[sprimId] = camera;
        return camera;
    }
    else if (typeId == HdPrimTypeTokens->material) {
        return new HydraPassthroughMaterial(sprimId);
    }
    else if (typeId == HdPrimTypeTokens->extComputation) {
        // The stock HdExtComputation syncs all the computation metadata we
        // need; the CPU evaluation happens in HydraPassthroughExtCompCpuComputation.
        return new HdExtComputation(sprimId);
    }

    // Should be unreachable
    TF_CODING_ERROR("Unknown Sprim type=%s id=%s", typeId.GetText(),
            sprimId.GetText());
    return nullptr;
}

HdSprim *HydraPassthroughRenderDelegate::CreateFallbackSprim(TfToken const &typeId) {
    if (typeId == HdPrimTypeTokens->camera) {
        return new HdCamera(SdfPath::EmptyPath());
    }
    else if (typeId == HdPrimTypeTokens->material) {
        return new HydraPassthroughMaterial(SdfPath::EmptyPath());
    }
    else if (typeId == HdPrimTypeTokens->extComputation) {
        return new HdExtComputation(SdfPath::EmptyPath());
    }

    // Should be unreachable
    TF_CODING_ERROR("Creating unknown fallback sprim type=%s", typeId.GetText());
    return nullptr;
}

void HydraPassthroughRenderDelegate::DestroySprim(HdSprim *sPrim) {
    if (dynamic_cast<HdCamera*>(sPrim)) {
        // Remove from our camera map if it's there
        auto it = _cameraMap.find(sPrim->GetId());
        if (it != _cameraMap.end()) {
            _cameraMap.erase(it);
        }
    }
    delete sPrim;
}

HdBprim *HydraPassthroughRenderDelegate::CreateBprim(TfToken const &typeId,
        SdfPath const &bprimId) {
    TF_CODING_ERROR("Unknown Bprim type=%s id=%s", typeId.GetText(),
            bprimId.GetText());
    return nullptr;
}

HdBprim *HydraPassthroughRenderDelegate::CreateFallbackBprim(TfToken const &typeId) {
    TF_CODING_ERROR("Creating unknown fallback bprim type=%s", typeId.GetText());
    return nullptr;
}

void HydraPassthroughRenderDelegate::DestroyBprim(HdBprim *bPrim) {
    TF_CODING_ERROR("Destroy Bprim not supported");
}

HdInstancer *HydraPassthroughRenderDelegate::CreateInstancer(HdSceneDelegate *delegate,
        SdfPath const &id) {
    return new HydraPassthroughInstancer(delegate, id);
}

void HydraPassthroughRenderDelegate::DestroyInstancer(HdInstancer *instancer) {
    delete instancer;
}

HdRenderParam *HydraPassthroughRenderDelegate::GetRenderParam() const
{ 
    return _renderParam.get();
}

HydraPassthroughRenderDataRefPtr HydraPassthroughRenderDelegate::GetRenderData() const {
    return _renderData;
}

PXR_NAMESPACE_CLOSE_SCOPE
