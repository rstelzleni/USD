#include "renderData.h"

#include "pxr/usdImaging/hydraPassthrough/constantVtBufferSource.h"
#include "pxr/usdImaging/hydraPassthrough/fvarTopologyTracker.h"
#include "pxr/usdImaging/hydraPassthrough/hdTypeUtil.h"
#include "pxr/usdImaging/hydraPassthrough/meshPackagingUtil.h"
#include "pxr/usdImaging/hydraPassthrough/subdivision.h"

#include "pxr/base/tf/diagnosticLite.h"
#include "pxr/imaging/hd/camera.h"
#include "pxr/imaging/hd/vtBufferSource.h"

#include "pxr/base/vt/value.h"

#include <numeric>


PXR_NAMESPACE_OPEN_SCOPE

namespace HdTypeUtil = HydraPassthroughHdTypeUtil;
namespace PackagingUtil = HydraPassthroughMeshPackagingUtil;

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

const HydraPassthroughRenderData::SceneGraphInstancerData*
HydraPassthroughRenderData::RenderData::GetSceneGraphInstancer(const SdfPath& id) const {
    auto it = sceneGraphInstancers.find(id);
    if (it != sceneGraphInstancers.end()) {
        return &(it->second);
    }
    return nullptr;
}

size_t
HydraPassthroughRenderData::RenderData::GetSceneGraphInstancerCount() const {
    return sceneGraphInstancers.size();
}

const HydraPassthroughRenderData::SceneGraphInstancerData*
HydraPassthroughRenderData::RenderData::GetSceneGraphInstancerByIndex(size_t index) const {
    if (index >= sceneGraphInstancers.size()) {
        TF_RUNTIME_ERROR("Instancer index %zu out of range [0,%zu)",
            index, sceneGraphInstancers.size());
        return nullptr;
    }

    auto it = sceneGraphInstancers.begin();
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

void
HydraPassthroughRenderData::AddSceneGraphInstancer(
                            const SdfPath& id,
                            const SceneGraphInstancerData& instancerData) {
    const std::lock_guard<std::mutex> lock(_instancerMutex);
    _renderData.sceneGraphInstancers[id] = instancerData;
}

static void _UpdateFaceVaryingChannels(HydraPassthroughRenderData::MeshData& meshData,
                                       const TfToken& primvarName) {
    // If this is a face varying primvar, we need to add it to the appropriate
    // channel in faceVaryingChannels

    // Find the channel for this primvar
    int channel = meshData.fvarTopologyTracker->GetChannelFromPrimvar(primvarName);
    if (channel < 0) {
        // Face-varying channels only exist for refined (subdivided) meshes.
        // For unrefined meshes the primvar buffer has already been
        // triangulated into one value per corner of faceVertexIndices, in
        // triangle order, so it needs no channel or index buffer.
        return;
    }

    // Find the channel if it exists, otherwise create a new one
    auto it = std::find_if(meshData.faceVaryingChannels.begin(), meshData.faceVaryingChannels.end(),
        [channel](const HydraPassthroughRenderData::FaceVaryingChannel& c) { return c.channel == channel; });
    if (it == meshData.faceVaryingChannels.end()) {
        // Create new channel
        meshData.faceVaryingChannels.push_back({channel, {}, {primvarName}});
    } else if (std::find(it->primvars.begin(), it->primvars.end(),
                         primvarName) == it->primvars.end()) {
        // Add primvar to existing channel
        it->primvars.push_back(primvarName);
    }
}

static void _UpdateFaceVaryingIndices(int channel, HydraPassthroughRenderData::MeshData& meshData, const VtValue& indices) {
    // This is an index buffer for a face varying primvar channel. We need to
    // add it to the appropriate channel in our output structure
    if (channel < 0) {
        TF_RUNTIME_ERROR("Invalid face varying channel %d for index buffer", channel);
        return;
    }

    // Find the channel if it exists, otherwise create a new one
    auto it = std::find_if(meshData.faceVaryingChannels.begin(), meshData.faceVaryingChannels.end(),
        [channel](const HydraPassthroughRenderData::FaceVaryingChannel& c) { return c.channel == channel; });
    if (it == meshData.faceVaryingChannels.end()) {
        // Create new channel with this index buffer
        meshData.faceVaryingChannels.push_back({channel, indices, {}});
    } else {
        // Add index buffer to existing channel
        it->indices = indices;
    }
}

void
HydraPassthroughRenderData::CopyPrimvarBufferSource(
        const SdfPath& id,
        HdBufferSourceSharedPtr const &source,
        HydraPassthroughResourceRegistry::PrimvarSourceType sourceType,
        HdInterpolation interpolation)
{
    // Early out if this is not a primvar, but ultimately we should handle points and indices
    // also maybe transform and transform inverse? others?
    if (sourceType != HydraPassthroughResourceRegistry::PrimvarSourceType::Primvar &&
        sourceType != HydraPassthroughResourceRegistry::PrimvarSourceType::Index) {
        return;
    }

    const std::lock_guard<std::mutex> lock(_meshMutex);
    auto meshIt = _renderData.meshes.find(id);
    if (meshIt != _renderData.meshes.end()) {

        if (source->GetNumElements() == 0) {
            // Empty buffers, like index buffers for meshes with no indices, don't 
            // need to be processed.
            return;
        }

        const TfToken& name = source->GetName();

        // Check for our special case types that preserve VtValues as is
        if (auto constantVtSource =
                std::dynamic_pointer_cast<
                HydraPassthroughConstantVtBufferSource>(source)) {
            const VtValue& value = constantVtSource->GetValue();
            meshIt->second.primvars[name] = { value, interpolation };
            return;
        }

        VtValue value(HdTypeUtil::CastRenderDataToCppType(source));

        // Special case names
        if (name == HdTokens->transform) {
            if (value.IsHolding<GfMatrix4d>()) {
                meshIt->second.transform = value.UncheckedGet<GfMatrix4d>();
            }
            else {
                TF_RUNTIME_ERROR("Expected transform to be of type GfMatrix4d, got %s", value.GetTypeName().c_str());
            }
            return;
        }
        else if (name == HdTokens->transformInverse) {
            if (value.IsHolding<GfMatrix4d>()) {
                meshIt->second.transformInverse = value.UncheckedGet<GfMatrix4d>();
            }
            else {
                TF_RUNTIME_ERROR("Expected transformInverse to be of type GfMatrix4d, got %s", value.GetTypeName().c_str());
            }
            return;
        }
        else if (name == HdTokens->points) {
            // Points is a primvar type, but we want to hold out a separate copy
            meshIt->second.points = value;
        }

        // Handle each source according to its type
        switch (sourceType) {
            case HydraPassthroughResourceRegistry::PrimvarSourceType::Index:
                {
                    // Check if this is an fvar index buffer.
                    //
                    // We don't use the fvar tracker to check this, because it tracks what channel
                    // a primvar is in. For instance, "st" is in channel 0. In this index buffer case
                    // we're getting "here are the indices for a channel" and we'll get a name like
                    // "fvarIndices0" instead of "st". So, ask the subdiv class for the channel number.
                    int channel = HydraPassthroughSubdivision::GetChannelFromPrimvarChannelIndexName(name);
                    if (channel >= 0) {
                        // This is an index buffer for a face varying primvar channel.
                        // We need to add it to the appropriate channel in our output structure
                        _UpdateFaceVaryingIndices(channel, meshIt->second, value);
                    }
                    else {
                        // Not an fvar index, it should be our face indices value
                        if (source->HasChainedBuffer()) {
                            // The chained buffers can contain additional helpful info about the mesh
                            // if it was subdivided
                            HdBufferSpecVector specs;
                            source->GetBufferSpecs(&specs);
                            // We can expect to get 3 of these with some expected names
                            if (specs.size() >= 3 &&
                                specs[0].name == HdTokens->indices&&
                                specs[1].name == HdTokens->primitiveParam &&
                                specs[2].name == HdTokens->edgeIndices) {

                                // The chained buffers will be the second and third items from specs
                                const HdBufferSourceSharedPtrVector &chainedBuffers =
                                    source->GetChainedBuffers();
                                const auto &primitiveParam = chainedBuffers[0];
                                const auto &edgeIndices = chainedBuffers[1];
                                meshIt->second.primitiveParam = HdTypeUtil::CastRenderDataToCppType(primitiveParam);
                                meshIt->second.edgeIndices = HdTypeUtil::CastRenderDataToCppType(edgeIndices);
                            }
                        }

                        // We can get the indices the same way for subdivs and non-subdivs
                        if (value.IsHolding<VtArray<int>>()) {
                            meshIt->second.faceVertexIndices = value.UncheckedGet<VtArray<int>>();
                        }
                        // An unsubdivided triangle mesh can use GfVec3i for its indices
                        else if (value.IsHolding<VtArray<GfVec3i>>()) {
                            const VtArray<GfVec3i>& vecIndices = value.UncheckedGet<VtArray<GfVec3i>>();
                            VtIntArray intIndices(vecIndices.size() * 3);
                            for (size_t i = 0; i < vecIndices.size(); ++i) {
                                intIndices[i*3] = vecIndices[i][0];
                                intIndices[i*3 + 1] = vecIndices[i][1];
                                intIndices[i*3 + 2] = vecIndices[i][2];
                            }
                            meshIt->second.faceVertexIndices = intIndices;
                        }
                        // An unsubdivided quad mesh would use GfVec4i for indices, but we don't support
                        // returning quads currently.
                        else {
                            TF_WARN("Expected index source to be of type VtArray<int> or VtArray<GfVec3i>, got %s for %s - %s",
                                    value.GetTypeName().c_str(),
                                    id.GetText(), name.GetText());
                        }
                    }
                }
                break;
            case HydraPassthroughResourceRegistry::PrimvarSourceType::Primvar:
                meshIt->second.primvars[name] = { value, interpolation };
                if (interpolation == HdInterpolationFaceVarying) {
                    _UpdateFaceVaryingChannels(meshIt->second, name);
                }
                break;
            case HydraPassthroughResourceRegistry::PrimvarSourceType::Generic:
                meshIt->second.primvars[name] = { value, interpolation };
                break;
        }
    } else {
        TF_RUNTIME_ERROR("Mesh with id %s not found when copying primvar buffer source", id.GetText());
    }
}

HydraPassthroughRenderData::RenderData
HydraPassthroughRenderData::ExtractRenderDataCopy() const {
    const std::lock_guard<std::mutex> lock1(_meshMutex);
    const std::lock_guard<std::mutex> lock2(_cameraMutex);
    const std::lock_guard<std::mutex> lock3(_materialMutex);
    const std::lock_guard<std::mutex> lock4(_instancerMutex);
    return _renderData;
}

HydraPassthroughRenderData::RenderData
HydraPassthroughRenderData::ExtractDeindexedRenderDataCopy() const {
    RenderData renderData = ExtractRenderDataCopy();
    for (auto& meshEntry : renderData.meshes) {
        PackagingUtil::DeindexMesh(&meshEntry.second);
    }
    return renderData;
}

HydraPassthroughRenderData::RenderData
HydraPassthroughRenderData::ExtractWeldedRenderDataCopy() const {
    RenderData renderData = ExtractRenderDataCopy();
    for (auto& meshEntry : renderData.meshes) {
        PackagingUtil::WeldMesh(&meshEntry.second);
    }
    return renderData;
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

const HydraPassthroughRenderData::SceneGraphInstancerData*
HydraPassthroughRenderData::GetSceneGraphInstancer(const SdfPath& id) const
{
    const std::lock_guard<std::mutex> lock(_instancerMutex);
    return _renderData.GetSceneGraphInstancer(id);
}

size_t
HydraPassthroughRenderData::GetSceneGraphInstancerCount() const
{
    const std::lock_guard<std::mutex> lock(_instancerMutex);
    return _renderData.GetSceneGraphInstancerCount();
}

const HydraPassthroughRenderData::SceneGraphInstancerData*
HydraPassthroughRenderData::GetSceneGraphInstancerByIndex(size_t index) const
{
    const std::lock_guard<std::mutex> lock(_instancerMutex);
    return _renderData.GetSceneGraphInstancerByIndex(index);
}

PXR_NAMESPACE_CLOSE_SCOPE
