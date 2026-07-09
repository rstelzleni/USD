#!/pxrpythonsubst

from pxr import Gf, HydraPassthrough, Sdf, Usd, UsdGeom, Vt
import unittest


def create_two_quad_plane_usd(stage):
    """Create an unsubdivided two-quad plane with a face-varying st primvar
    that has a seam between the two faces, plus vertex and uniform primvars.
    """
    mesh = UsdGeom.Mesh.Define(stage, '/Plane')
    mesh.CreateSubdivisionSchemeAttr(UsdGeom.Tokens.none)

    points = [(0, 0, 0), (1, 0, 0), (1, 1, 0), (0, 1, 0),
              (2, 0, 0), (2, 1, 0)]
    mesh.CreatePointsAttr(Vt.Vec3fArray(points))
    mesh.CreateFaceVertexCountsAttr(Vt.IntArray([4, 4]))
    mesh.CreateFaceVertexIndicesAttr(Vt.IntArray([0, 1, 2, 3, 1, 4, 5, 2]))

    primvars = UsdGeom.PrimvarsAPI(mesh)

    # Four st values per face, discontinuous across the shared edge
    st = [(0.0, 0.0), (0.5, 0.0), (0.5, 1.0), (0.0, 1.0),
          (0.6, 0.0), (1.0, 0.0), (1.0, 1.0), (0.6, 1.0)]
    st_primvar = primvars.CreatePrimvar(
        'st', Sdf.ValueTypeNames.TexCoord2fArray, UsdGeom.Tokens.faceVarying)
    st_primvar.Set(Vt.Vec2fArray(st))

    colors = [(i / 10.0, 0.0, 0.0) for i in range(len(points))]
    color_primvar = primvars.CreatePrimvar(
        'displayColor', Sdf.ValueTypeNames.Color3fArray, UsdGeom.Tokens.vertex)
    color_primvar.Set(Vt.Vec3fArray(colors))

    uniform_primvar = primvars.CreatePrimvar(
        'myUniform', Sdf.ValueTypeNames.FloatArray, UsdGeom.Tokens.uniform)
    uniform_primvar.Set(Vt.FloatArray([10.0, 20.0]))

    return points, st


class TestMeshData(unittest.TestCase):
    def test_Basic(self):
        stage = Usd.Stage.CreateInMemory()
        UsdGeom.Mesh.Define(stage, '/Mesh0')
        UsdGeom.Mesh.Define(stage, '/Mesh1')
        UsdGeom.Mesh.Define(stage, '/Mesh2')
        m = HydraPassthrough.RenderManager()
        m.Initialize()
        m.Render(stage)

        md = m.GetRenderData()

        prefix = HydraPassthrough.RenderManager.GetSceneDelegateId()

        self.assertEqual(md.GetMeshCount(), 3)
        mesh0 = md.GetMesh(Sdf.Path(prefix.AppendPath('Mesh0')))
        self.assertEqual(mesh0.id, Sdf.Path(prefix.AppendPath('Mesh0')))

        # Make sure all the getters work and translate to python
        x = mesh0.visible
        self.assertEqual(x, True)
        x = mesh0.doubleSided
        self.assertEqual(x, False)
        x = mesh0.transform
        self.assertEqual(x, Gf.Matrix4d(1))
        x = mesh0.GetPoints()
        self.assertEqual(x.GetValue(), None)
        x = mesh0.GetFaceVertexIndices()
        self.assertEqual(x.GetValue(), [])
        x = mesh0.materialId
        self.assertEqual(x, Sdf.Path())
        x = mesh0.GetAllPrimvars()
        self.assertEqual(x, [])
        x = mesh0.GetPrimvar('nonexistent')
        self.assertEqual(x, None)

        m.Cleanup()

    def test_ExtractData(self):
        stage = Usd.Stage.CreateInMemory()
        UsdGeom.Mesh.Define(stage, '/Mesh')
        m = HydraPassthrough.RenderManager()
        m.Initialize()
        try:
            m.Render(stage)

            md = m.GetRenderData()

            prefix = HydraPassthrough.RenderManager.GetSceneDelegateId()

            self.assertEqual(md.GetMeshCount(), 1)
            mesh = md.GetMesh(Sdf.Path(prefix.AppendPath('Mesh')))
            self.assertEqual(mesh.id, Sdf.Path(prefix.AppendPath('Mesh')))

            data_copy = md.ExtractRenderDataCopy()
        finally:
            m.Cleanup()

        # Test that the copied data is valid and has the same contents as the
        # original data.
        self.assertEqual(data_copy.GetMeshCount(), 1)
        meshCopy = data_copy.GetMesh(Sdf.Path(prefix.AppendPath('Mesh')))
        self.assertEqual(meshCopy.id, Sdf.Path(prefix.AppendPath('Mesh')))

        # Test that the old md object is not longer valid after Cleanup(). Calling
        # any function on it raises a Boost.Python.ArgumentError, which is just an
        # Exception in Python so we can't catch a more specific type. 
        #
        # Error looks like
        #
        # Traceback (most recent call last):
        #   File "/opt/USD/tests/testHydraPassthroughMeshData", line 72, in test_ExtractData
        #     self.assertEqual(md.GetMeshCount(), 1)
        #                      ^^^^^^^^^^^^^^^^^
        # Boost.Python.ArgumentError: Python argument types in
        #     RenderData.GetMeshCount(RenderData)
        # did not match C++ signature:
        #     GetMeshCount(pxrInternal_v0_25_5__pxrReserved__::HydraPassthroughRenderData {lvalue})
        with self.assertRaises(Exception):
            self.assertEqual(md.GetMeshCount(), 1)

    def test_UnrefinedFaceVarying(self):
        # An unsubdivided mesh with a face-varying primvar: the primvar is
        # triangulated into one value per corner and needs no face-varying
        # channel.
        stage = Usd.Stage.CreateInMemory()
        points, st = create_two_quad_plane_usd(stage)

        m = HydraPassthrough.RenderManager()
        m.Initialize()
        try:
            m.Render(stage)
            data_copy = m.GetRenderData().ExtractRenderDataCopy()
        finally:
            m.Cleanup()

        prefix = HydraPassthrough.RenderManager.GetSceneDelegateId()
        mesh = data_copy.GetMesh(Sdf.Path(prefix.AppendPath('Plane')))
        self.assertIsNotNone(mesh)

        # Each quad (a, b, c, d) is fan-triangulated into (a, b, c), (a, c, d)
        expected_corner_verts = [0, 1, 2, 0, 2, 3, 1, 4, 5, 1, 5, 2]
        expected_st_corners = [0, 1, 2, 0, 2, 3, 4, 5, 6, 4, 6, 7]

        self.assertEqual(list(mesh.GetFaceVertexIndices().GetValue()),
                         expected_corner_verts)

        # Points are untouched by the plain copy
        self.assertEqual(len(mesh.GetPoints().GetValue()), len(points))

        # The st primvar is already triangulated per corner, with no channel
        st_primvar = mesh.GetPrimvar('st')
        self.assertEqual(st_primvar.interpolation, UsdGeom.Tokens.faceVarying)
        self.assertEqual(list(st_primvar.data.GetValue()),
                         [Gf.Vec2f(*st[i]) for i in expected_st_corners])
        self.assertEqual(mesh.GetFaceVaryingChannels(), [])

    def test_DeindexedExtraction(self):
        # ExtractDeindexedRenderDataCopy expands every non-constant primvar
        # to one value per corner, in corner order, with identity indices.
        stage = Usd.Stage.CreateInMemory()
        points, st = create_two_quad_plane_usd(stage)

        m = HydraPassthrough.RenderManager()
        m.Initialize()
        try:
            m.Render(stage)
            data_copy = m.GetRenderData().ExtractDeindexedRenderDataCopy()
        finally:
            m.Cleanup()

        prefix = HydraPassthrough.RenderManager.GetSceneDelegateId()
        mesh = data_copy.GetMesh(Sdf.Path(prefix.AppendPath('Plane')))
        self.assertIsNotNone(mesh)

        expected_corner_verts = [0, 1, 2, 0, 2, 3, 1, 4, 5, 1, 5, 2]
        expected_st_corners = [0, 1, 2, 0, 2, 3, 4, 5, 6, 4, 6, 7]
        num_corners = len(expected_corner_verts)

        # Indices become the identity
        self.assertEqual(list(mesh.GetFaceVertexIndices().GetValue()),
                         list(range(num_corners)))

        # Points are expanded per corner
        self.assertEqual(list(mesh.GetPoints().GetValue()),
                         [Gf.Vec3f(*points[i]) for i in expected_corner_verts])

        # Face-varying st is unchanged (already per corner)
        st_primvar = mesh.GetPrimvar('st')
        self.assertEqual(st_primvar.interpolation, UsdGeom.Tokens.faceVarying)
        self.assertEqual(list(st_primvar.data.GetValue()),
                         [Gf.Vec2f(*st[i]) for i in expected_st_corners])

        # Vertex displayColor is expanded per corner and reports faceVarying
        color_primvar = mesh.GetPrimvar('displayColor')
        self.assertEqual(color_primvar.interpolation,
                         UsdGeom.Tokens.faceVarying)
        self.assertEqual(
            list(color_primvar.data.GetValue()),
            [Gf.Vec3f(i / 10.0, 0.0, 0.0) for i in expected_corner_verts])

        # Uniform primvars are expanded per corner of each source face
        uniform_primvar = mesh.GetPrimvar('myUniform')
        self.assertEqual(uniform_primvar.interpolation,
                         UsdGeom.Tokens.faceVarying)
        self.assertEqual(list(uniform_primvar.data.GetValue()),
                         [10.0] * 6 + [20.0] * 6)

        # The channels are consumed by de-indexing
        self.assertEqual(mesh.GetFaceVaryingChannels(), [])

    def test_WeldedExtraction(self):
        # ExtractWeldedRenderDataCopy produces the same single-index
        # contract as de-indexing, but shares vertices wherever all
        # attributes agree, splitting only across seams.
        stage = Usd.Stage.CreateInMemory()
        points, st = create_two_quad_plane_usd(stage)

        m = HydraPassthrough.RenderManager()
        m.Initialize()
        try:
            m.Render(stage)
            data_copy = m.GetRenderData().ExtractWeldedRenderDataCopy()
        finally:
            m.Cleanup()

        prefix = HydraPassthrough.RenderManager.GetSceneDelegateId()
        mesh = data_copy.GetMesh(Sdf.Path(prefix.AppendPath('Plane')))
        self.assertIsNotNone(mesh)

        # 12 corners weld into 8 vertices: within each quad the two
        # triangles share their diagonal corners, but the seam in st (and
        # the uniform primvar) splits the two vertices shared between the
        # quads. Output vertices are numbered in order of first use, so
        # the welded index buffer is deterministic.
        expected_indices = [0, 1, 2, 0, 2, 3, 4, 5, 6, 4, 6, 7]
        # Source vertex and st element feeding each welded vertex
        welded_verts = [0, 1, 2, 3, 1, 4, 5, 2]
        welded_sts = [0, 1, 2, 3, 4, 5, 6, 7]

        self.assertEqual(list(mesh.GetFaceVertexIndices().GetValue()),
                         expected_indices)

        self.assertEqual(list(mesh.GetPoints().GetValue()),
                         [Gf.Vec3f(*points[i]) for i in welded_verts])

        # All welded primvars are parallel to points and report vertex
        # interpolation
        st_primvar = mesh.GetPrimvar('st')
        self.assertEqual(st_primvar.interpolation, UsdGeom.Tokens.vertex)
        self.assertEqual(list(st_primvar.data.GetValue()),
                         [Gf.Vec2f(*st[i]) for i in welded_sts])

        color_primvar = mesh.GetPrimvar('displayColor')
        self.assertEqual(color_primvar.interpolation, UsdGeom.Tokens.vertex)
        self.assertEqual(
            list(color_primvar.data.GetValue()),
            [Gf.Vec3f(i / 10.0, 0.0, 0.0) for i in welded_verts])

        uniform_primvar = mesh.GetPrimvar('myUniform')
        self.assertEqual(uniform_primvar.interpolation,
                         UsdGeom.Tokens.vertex)
        self.assertEqual(list(uniform_primvar.data.GetValue()),
                         [10.0] * 4 + [20.0] * 4)

        # The channels are consumed by welding
        self.assertEqual(mesh.GetFaceVaryingChannels(), [])

    def test_WeldedExtractionContinuousUvs(self):
        # When the face-varying data has no seams, welding recovers plain
        # vertex sharing: no splits, uv array length matches points.
        stage = Usd.Stage.CreateInMemory()
        points, _ = create_two_quad_plane_usd(stage)

        # Overwrite st with values that are continuous across the shared
        # edge (one distinct value per vertex of each corner)
        mesh_prim = UsdGeom.Mesh(stage.GetPrimAtPath('/Plane'))
        vertex_indices = [0, 1, 2, 3, 1, 4, 5, 2]
        vertex_uvs = [(0.0, 0.0), (0.4, 0.0), (0.4, 1.0), (0.0, 1.0),
                      (1.0, 0.0), (1.0, 1.0)]
        continuous_st = [vertex_uvs[i] for i in vertex_indices]
        st_primvar = UsdGeom.PrimvarsAPI(mesh_prim).GetPrimvar('st')
        st_primvar.Set(Vt.Vec2fArray(continuous_st))

        # Drop the uniform primvar so nothing else forces splits
        UsdGeom.PrimvarsAPI(mesh_prim).RemovePrimvar('myUniform')

        m = HydraPassthrough.RenderManager()
        m.Initialize()
        try:
            m.Render(stage)
            data_copy = m.GetRenderData().ExtractWeldedRenderDataCopy()
        finally:
            m.Cleanup()

        prefix = HydraPassthrough.RenderManager.GetSceneDelegateId()
        mesh = data_copy.GetMesh(Sdf.Path(prefix.AppendPath('Plane')))
        self.assertIsNotNone(mesh)

        # Six vertices in, six vertices out: continuous uvs cost nothing.
        # Welded vertices are numbered in order of first use by the
        # triangulated corners [0,1,2, 0,2,3, 1,4,5, 1,5,2].
        self.assertEqual(len(mesh.GetPoints().GetValue()), len(points))
        self.assertEqual(list(mesh.GetFaceVertexIndices().GetValue()),
                         [0, 1, 2, 0, 2, 3, 1, 4, 5, 1, 5, 2])

        st_welded = mesh.GetPrimvar('st')
        self.assertEqual(st_welded.interpolation, UsdGeom.Tokens.vertex)
        self.assertEqual(list(st_welded.data.GetValue()),
                         [Gf.Vec2f(*vertex_uvs[i]) for i in range(6)])

    def test_TriangleMeshFaceVarying(self):
        # A mesh that is already all triangles: HdMeshUtil reports the
        # face-varying triangulation as "Unchanged" and the source buffer
        # must be passed through rather than dropped. This is the common
        # case for usdz assets, which are typically pre-triangulated.
        stage = Usd.Stage.CreateInMemory()
        mesh_prim = UsdGeom.Mesh.Define(stage, '/Tris')
        mesh_prim.CreateSubdivisionSchemeAttr(UsdGeom.Tokens.none)
        points = [(0, 0, 0), (1, 0, 0), (1, 1, 0), (0, 1, 0)]
        mesh_prim.CreatePointsAttr(Vt.Vec3fArray(points))
        mesh_prim.CreateFaceVertexCountsAttr(Vt.IntArray([3, 3]))
        mesh_prim.CreateFaceVertexIndicesAttr(
            Vt.IntArray([0, 1, 2, 0, 2, 3]))

        # Indexed st, like typical pre-triangulated exports
        compact_uvs = [(0.0, 0.0), (1.0, 0.0), (1.0, 1.0), (0.0, 1.0)]
        st_indices = [0, 1, 2, 0, 2, 3]
        st_primvar = UsdGeom.PrimvarsAPI(mesh_prim).CreatePrimvar(
            'st', Sdf.ValueTypeNames.TexCoord2fArray,
            UsdGeom.Tokens.faceVarying)
        st_primvar.Set(Vt.Vec2fArray(compact_uvs))
        st_primvar.SetIndices(Vt.IntArray(st_indices))

        m = HydraPassthrough.RenderManager()
        m.Initialize()
        try:
            m.Render(stage)
            data_copy = m.GetRenderData().ExtractRenderDataCopy()
        finally:
            m.Cleanup()

        prefix = HydraPassthrough.RenderManager.GetSceneDelegateId()
        mesh = data_copy.GetMesh(Sdf.Path(prefix.AppendPath('Tris')))
        self.assertIsNotNone(mesh)

        # The topology passes through untriangulated
        self.assertEqual(list(mesh.GetFaceVertexIndices().GetValue()),
                         [0, 1, 2, 0, 2, 3])

        # st must be present: the flattened source is already per corner
        st_out = mesh.GetPrimvar('st')
        self.assertIsNotNone(st_out)
        self.assertEqual(st_out.interpolation, UsdGeom.Tokens.faceVarying)
        self.assertEqual(list(st_out.data.GetValue()),
                         [Gf.Vec2f(*compact_uvs[i]) for i in st_indices])

    def test_WeldedExtractionIndexedUvs(self):
        # An indexed face-varying primvar on an unrefined mesh: the scene
        # delegate hands the pipeline the flattened value, so authored
        # indices are invisible downstream and welding behaves exactly as
        # for a non-indexed primvar with the same effective values.
        stage = Usd.Stage.CreateInMemory()
        points, _ = create_two_quad_plane_usd(stage)

        # Author st as a compact value array plus indices, continuous
        # across the shared edge (corners of both faces reference the same
        # elements there)
        mesh_prim = UsdGeom.Mesh(stage.GetPrimAtPath('/Plane'))
        compact_uvs = [(0.0, 0.0), (0.5, 0.0), (0.5, 1.0), (0.0, 1.0),
                       (1.0, 0.0), (1.0, 1.0)]
        st_primvar = UsdGeom.PrimvarsAPI(mesh_prim).GetPrimvar('st')
        st_primvar.Set(Vt.Vec2fArray(compact_uvs))
        st_primvar.SetIndices(Vt.IntArray([0, 1, 2, 3, 1, 4, 5, 2]))

        # Drop the uniform primvar so nothing else forces splits
        UsdGeom.PrimvarsAPI(mesh_prim).RemovePrimvar('myUniform')

        m = HydraPassthrough.RenderManager()
        m.Initialize()
        try:
            m.Render(stage)
            data_copy = m.GetRenderData().ExtractWeldedRenderDataCopy()
        finally:
            m.Cleanup()

        prefix = HydraPassthrough.RenderManager.GetSceneDelegateId()
        mesh = data_copy.GetMesh(Sdf.Path(prefix.AppendPath('Plane')))
        self.assertIsNotNone(mesh)

        # The uvs are continuous, so welding recovers full vertex sharing
        self.assertEqual(len(mesh.GetPoints().GetValue()), len(points))
        self.assertEqual(list(mesh.GetFaceVertexIndices().GetValue()),
                         [0, 1, 2, 0, 2, 3, 1, 4, 5, 1, 5, 2])

        st_welded = mesh.GetPrimvar('st')
        self.assertIsNotNone(st_welded)
        self.assertEqual(st_welded.interpolation, UsdGeom.Tokens.vertex)
        self.assertEqual(list(st_welded.data.GetValue()),
                         [Gf.Vec2f(*compact_uvs[i]) for i in range(6)])

    def test_DoubleSidedMeshes(self):
        # Test that double-sided meshes are correctly reported in the render data.
        stage = Usd.Stage.CreateInMemory()
        mesh_prim = UsdGeom.Mesh.Define(stage, '/DoubleSidedMesh')
        mesh_prim.CreateSubdivisionSchemeAttr(UsdGeom.Tokens.none)
        mesh_prim.CreateDoubleSidedAttr(True)

        m = HydraPassthrough.RenderManager()
        m.Initialize()
        try:
            m.Render(stage)
            data_copy = m.GetRenderData().ExtractRenderDataCopy()
        finally:
            m.Cleanup()

        prefix = HydraPassthrough.RenderManager.GetSceneDelegateId()
        mesh = data_copy.GetMesh(Sdf.Path(prefix.AppendPath('DoubleSidedMesh')))
        self.assertIsNotNone(mesh)
        self.assertTrue(mesh.doubleSided)


if __name__ == "__main__":
    unittest.main()
