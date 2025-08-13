#pragma once

#include "pxr/pxr.h"

#include "pxr/imaging/hd/enums.h"
#include "pxr/imaging/hd/meshTopology.h"

#include "pxr/usd/sdf/path.h"

#include "pxr/base/gf/matrix4f.h"
#include "pxr/base/tf/declarePtrs.h"
#include "pxr/base/tf/hashmap.h"
#include "pxr/base/vt/array.h"
#include "pxr/base/vt/value.h"

#include <string>

PXR_NAMESPACE_OPEN_SCOPE

TF_DECLARE_REF_PTRS(HydraPassthroughRenderData);

class HydraPassthroughRenderData :
    public TfRefBase,
    public TfWeakBase
{
public:
    struct PrimvarSource {
        VtValue data;
        HdInterpolation interpolation;
    };
    class MeshData {
    public:
        SdfPath id;
        bool visible = true;
        GfMatrix4f transform;
        VtVec3fArray points;
        //VtVec3fArray normals;
        //VtVec2fArray uvs;
        VtVec3iArray faceVertexIndices; // triangles only

        // Additional data for triangulation.
        VtIntArray triangleOriginalFaceIndices;

        // edges encoded like (I believe these are the only values, due to the
        // triangulation approach)
        //  0        show all edges
        //  1        hide edge [2-0]
        //  2        hide edge [0-1]
        //  3        hide edges [0-1] and [2-0]
        VtIntArray triangleEdgeIndices;

        // Not available in python
        HdMeshTopology topology;
        TfHashMap<TfToken, PrimvarSource, TfToken::HashFunctor> primvarSourceMap;
    };

    static HydraPassthroughRenderDataRefPtr New() {
        return TfCreateRefPtr(new HydraPassthroughRenderData());
    }

    ~HydraPassthroughRenderData() = default;

    void AddMesh(const SdfPath& id,
                 const MeshData& meshData);

    size_t GetMeshCount() const {
        return _meshes.size();
    }

    const MeshData& GetMesh(const SdfPath& id) const;

    const MeshData& GetMeshByIndex(size_t index) const;

private:
    HydraPassthroughRenderData() = default;

    // A map to store mesh data by their SdfPath identifiers.
    TfHashMap<SdfPath, MeshData, TfHash> _meshes;

    // Default mesh data to return in case of errors.
    MeshData _defaultMeshData;
};

PXR_NAMESPACE_CLOSE_SCOPE
