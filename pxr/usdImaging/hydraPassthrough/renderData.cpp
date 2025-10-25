#include "renderData.h"

#include "pxr/base/tf/diagnosticLite.h"
#include "pxr/imaging/hd/camera.h"

PXR_NAMESPACE_OPEN_SCOPE

const HydraPassthroughRenderData::MeshData*
HydraPassthroughRenderData::RenderData::GetMesh(const SdfPath& id) const {
    auto it = meshes.find(id);
    if (it != meshes.end()) {
        return &(it->second);
    }
    return nullptr;
}

size_t
HydraPassthroughRenderData::RenderData::GetMeshCount() const {
    return meshes.size();
}

const HydraPassthroughRenderData::MeshData*
HydraPassthroughRenderData::RenderData::GetMeshByIndex(size_t index) const {
    if (index < meshes.size()) {
        auto it = meshes.begin();
        std::advance(it, index);
        return &(it->second);
    }
    TF_RUNTIME_ERROR(
        "Index %zu out of bounds for HydraPassthroughRenderData with %zu meshes.",
        index, meshes.size());
    return nullptr;
}

const HydraPassthroughRenderData::CameraData*
HydraPassthroughRenderData::RenderData::GetCamera(const SdfPath& id) const {
    auto it = cameras.find(id);
    if (it != cameras.end()) {
        return &(it->second);
    }
    return nullptr;
}

size_t
HydraPassthroughRenderData::RenderData::GetCameraCount() const {
    return cameras.size();
}

const HydraPassthroughRenderData::CameraData* 
HydraPassthroughRenderData::RenderData::GetCameraByIndex(size_t index) const {
    if (index >= cameras.size()) {
        TF_RUNTIME_ERROR("Camera index %zu out of range [0,%zu)",
            index, cameras.size());
        return nullptr;
    }

    auto it = cameras.begin();
    std::advance(it, index);
    return &(it->second);
}

const HydraPassthroughRenderData::MaterialData* 
HydraPassthroughRenderData::RenderData::GetMaterial(const SdfPath& id) const {
    auto it = materials.find(id);
    if (it != materials.end()) {
        return &(it->second);
    }
    return nullptr;
}

size_t
HydraPassthroughRenderData::RenderData::GetMaterialCount() const {
    return materials.size();
}

const HydraPassthroughRenderData::MaterialData* 
HydraPassthroughRenderData::RenderData::GetMaterialByIndex(size_t index) const {
    if (index >= materials.size()) {
        TF_RUNTIME_ERROR("Material index %zu out of range [0,%zu)",
            index, materials.size());
        return nullptr;
    }

    auto it = materials.begin();
    std::advance(it, index);
    return &(it->second);
}

void
HydraPassthroughRenderData::AddMesh(
    const SdfPath& id,
    const MeshData& meshData) {
    const std::lock_guard<std::mutex> lock(_meshMutex);
    _renderData.meshes[id] = meshData;
}

void
HydraPassthroughRenderData::AddCamera(const HdCamera* camera) {
    if (!camera) {
        TF_RUNTIME_ERROR("Null camera pointer in passthrough AddCamera");
        return;
    }

    // XXX RYANS see also HdSceneDelegate::GetCameraParamValue
    CameraData camData;
    camData.id = camera->GetId();
    camData.transform = camera->GetTransform();
    camData.projectionMatrix = camera->ComputeProjectionMatrix();
    camData.projection = camera->GetProjection() == HdCamera::Perspective ?
        CameraData::Projection::Perspective : CameraData::Projection::Orthographic;
    camData.horizontalAperture = camera->GetHorizontalAperture();
    camData.verticalAperture = camera->GetVerticalAperture();
    camData.horizontalApertureOffset = camera->GetHorizontalApertureOffset();
    camData.verticalApertureOffset = camera->GetVerticalApertureOffset();
    camData.focalLength = camera->GetFocalLength();
    camData.clippingRange = camera->GetClippingRange();
    camData.clipPlanes = camera->GetClipPlanes();
    camData.fStop = camera->GetFStop();
    camData.focusDistance = camera->GetFocusDistance();
    camData.focusOn = camera->GetFocusOn();
    camData.dofAspect = camera->GetDofAspect();
    camData.splitDiopterCount = camera->GetSplitDiopterCount();
    camData.splitDiopterAngle = camera->GetSplitDiopterAngle();
    camData.splitDiopterOffset1 = camera->GetSplitDiopterOffset1();
    camData.splitDiopterWidth1 = camera->GetSplitDiopterWidth1();
    camData.splitDiopterFocusDistance1 = camera->GetSplitDiopterFocusDistance1();
    camData.splitDiopterOffset2 = camera->GetSplitDiopterOffset2();
    camData.splitDiopterWidth2 = camera->GetSplitDiopterWidth2();
    camData.splitDiopterFocusDistance2 = camera->GetSplitDiopterFocusDistance2();
    camData.shutterOpen = camera->GetShutterOpen();
    camData.shutterClose = camera->GetShutterClose();
    camData.linearExposureScale = camera->GetLinearExposureScale();
    camData.lensDistortionType = camera->GetLensDistortionType();
    camData.lensDistortionK1 = camera->GetLensDistortionK1();
    camData.lensDistortionK2 = camera->GetLensDistortionK2();
    camData.lensDistortionCenter = camera->GetLensDistortionCenter();
    camData.lensDistortionAnaSq = camera->GetLensDistortionAnaSq();
    camData.lensDistortionAsym = camera->GetLensDistortionAsym();
    camData.lensDistortionScale = camera->GetLensDistortionScale();
    camData.lensDistortionIor = camera->GetLensDistortionIor();
    camData.linearExposureScale = camera->GetLinearExposureScale();

    switch (camera->GetWindowPolicy()) {
        case CameraUtilMatchVertically:
            camData.windowPolicy = CameraData::WindowPolicy::MatchVertically;
            break;
        case CameraUtilMatchHorizontally:
            camData.windowPolicy = CameraData::WindowPolicy::MatchHorizontally;
            break;
        case CameraUtilFit:
            camData.windowPolicy = CameraData::WindowPolicy::Fit;
            break;
        case CameraUtilCrop:
            camData.windowPolicy = CameraData::WindowPolicy::Crop;
            break;
        case CameraUtilDontConform:
            camData.windowPolicy = CameraData::WindowPolicy::None;
            break;
    }

    {
        const std::lock_guard<std::mutex> lock(_cameraMutex);
        _renderData.cameras[camData.id] = camData;
    }
}

void
HydraPassthroughRenderData::AddMaterial(const SdfPath& id, 
                                        const MaterialData& matData) {
    const std::lock_guard<std::mutex> lock(_materialMutex);
    _renderData.materials[id] = matData;
}

HydraPassthroughRenderData::RenderData
HydraPassthroughRenderData::ExtractRenderDataCopy() const {
    const std::lock_guard<std::mutex> lock1(_meshMutex);
    const std::lock_guard<std::mutex> lock2(_cameraMutex);
    const std::lock_guard<std::mutex> lock3(_materialMutex);
    return _renderData;
}

size_t
HydraPassthroughRenderData::GetMeshCount() const
{
    const std::lock_guard<std::mutex> lock(_meshMutex);
    return _renderData.GetMeshCount();
}

const HydraPassthroughRenderData::MeshData*
HydraPassthroughRenderData::GetMesh(const SdfPath& id) const
{
    const std::lock_guard<std::mutex> lock(_meshMutex);
    return _renderData.GetMesh(id);
}

const HydraPassthroughRenderData::MeshData*
HydraPassthroughRenderData::GetMeshByIndex(size_t index) const
{
    const std::lock_guard<std::mutex> lock(_meshMutex);
    return _renderData.GetMeshByIndex(index);
}

const HydraPassthroughRenderData::CameraData*
HydraPassthroughRenderData::GetCamera(const SdfPath& id) const
{
    const std::lock_guard<std::mutex> lock(_cameraMutex);
    return _renderData.GetCamera(id);
}

size_t
HydraPassthroughRenderData::GetCameraCount() const
{
    const std::lock_guard<std::mutex> lock(_cameraMutex);
    return _renderData.GetCameraCount();
}

const HydraPassthroughRenderData::CameraData*
HydraPassthroughRenderData::GetCameraByIndex(size_t index) const
{
    const std::lock_guard<std::mutex> lock(_cameraMutex);
    return _renderData.GetCameraByIndex(index);
}

const HydraPassthroughRenderData::MaterialData*
HydraPassthroughRenderData::GetMaterial(const SdfPath& id) const
{
    const std::lock_guard<std::mutex> lock(_materialMutex);
    return _renderData.GetMaterial(id);
}

size_t
HydraPassthroughRenderData::GetMaterialCount() const
{
    const std::lock_guard<std::mutex> lock(_materialMutex);
    return _renderData.GetMaterialCount();
}

const HydraPassthroughRenderData::MaterialData *
HydraPassthroughRenderData::GetMaterialByIndex(size_t index) const
{
    const std::lock_guard<std::mutex> lock(_materialMutex);
    return _renderData.GetMaterialByIndex(index);
}

PXR_NAMESPACE_CLOSE_SCOPE
