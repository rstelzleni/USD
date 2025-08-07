#include "renderDelegate.h"
#include "mesh.h"
#include "renderPass.h"

#include <iostream>

PXR_NAMESPACE_OPEN_SCOPE

const TfTokenVector HydraPassthroughRenderDelegate::SUPPORTED_RPRIM_TYPES = {
    HdPrimTypeTokens->mesh,
};

const TfTokenVector HydraPassthroughRenderDelegate::SUPPORTED_SPRIM_TYPES = {};

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
    std::cout << "Creating Passthrough RenderDelegate" << std::endl;
    _resourceRegistry = std::make_shared<HdResourceRegistry>();
}

HydraPassthroughRenderDelegate::~HydraPassthroughRenderDelegate() {
    _resourceRegistry.reset();
    std::cout << "Destroying Passthrough RenderDelegate" << std::endl;
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
    std::cout << "=> CommitResources RenderDelegate" << std::endl;
}

HdRenderPassSharedPtr
HydraPassthroughRenderDelegate::CreateRenderPass(HdRenderIndex *index,
        HdRprimCollection const &collection) {
    std::cout << "Create RenderPass with Collection=" << collection.GetName()
        << std::endl;

    return HdRenderPassSharedPtr(new HydraPassthroughRenderPass(index, collection));
}

HdRprim *HydraPassthroughRenderDelegate::CreateRprim(TfToken const &typeId,
        SdfPath const &rprimId) {
    std::cout << "Create Passthrough Rprim type=" << typeId.GetText()
        << " id=" << rprimId << std::endl;

    if (typeId == HdPrimTypeTokens->mesh) {
        return new HydraPassthroughMesh(rprimId);
    } else {
        TF_CODING_ERROR("Unknown Rprim type=%s id=%s", typeId.GetText(),
                rprimId.GetText());
    }
    return nullptr;
}

void HydraPassthroughRenderDelegate::DestroyRprim(HdRprim *rPrim) {
    std::cout << "Destroy Passthrough Rprim id=" << rPrim->GetId() << std::endl;
    delete rPrim;
}

HdSprim *HydraPassthroughRenderDelegate::CreateSprim(TfToken const &typeId,
        SdfPath const &sprimId) {
    TF_CODING_ERROR("Unknown Sprim type=%s id=%s", typeId.GetText(),
            sprimId.GetText());
    return nullptr;
}

HdSprim *HydraPassthroughRenderDelegate::CreateFallbackSprim(TfToken const &typeId) {
    TF_CODING_ERROR("Creating unknown fallback sprim type=%s", typeId.GetText());
    return nullptr;
}

void HydraPassthroughRenderDelegate::DestroySprim(HdSprim *sPrim) {
    TF_CODING_ERROR("Destroy Sprim not supported");
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
    TF_CODING_ERROR("Creating Instancer not supported id=%s", id.GetText());
    return nullptr;
}

void HydraPassthroughRenderDelegate::DestroyInstancer(HdInstancer *instancer) {
    TF_CODING_ERROR("Destroy instancer not supported");
}

HdRenderParam *HydraPassthroughRenderDelegate::GetRenderParam() const { return nullptr; }

PXR_NAMESPACE_CLOSE_SCOPE
