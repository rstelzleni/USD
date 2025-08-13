#include "renderData.h"

#include "pxr/base/tf/diagnosticLite.h"

PXR_NAMESPACE_OPEN_SCOPE

void
HydraPassthroughRenderData::AddMesh(
    const SdfPath& id,
    const MeshData& meshData) {
    _meshes[id] = meshData;
}

const HydraPassthroughRenderData::MeshData&
HydraPassthroughRenderData::GetMesh(const SdfPath& id) const {
    auto it = _meshes.find(id);
    if (it != _meshes.end()) {
        return it->second;
    }
    TF_RUNTIME_ERROR(
        "Mesh with id '%s' not found in HydraPassthroughRenderData.",
        id.GetText());
    return _defaultMeshData;
}

const HydraPassthroughRenderData::MeshData&
HydraPassthroughRenderData::GetMeshByIndex(size_t index) const {
    if (index < _meshes.size()) {
        auto it = _meshes.begin();
        std::advance(it, index);
        return it->second;
    }
    TF_RUNTIME_ERROR(
        "Index %zu out of bounds for HydraPassthroughRenderData with %zu meshes.",
        index, _meshes.size());
    return _defaultMeshData;
}

PXR_NAMESPACE_CLOSE_SCOPE
