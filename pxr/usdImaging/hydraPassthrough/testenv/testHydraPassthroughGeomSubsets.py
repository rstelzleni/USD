#!/pxrpythonsubst

import math
import unittest

from pxr import HydraPassthrough, Sdf, Usd, UsdGeom, UsdShade, Vt

# The test plane: three quads and a triangle in a strip.
#
#   5---6---7---8
#   |   |   |   | \
#   |   |   |   |  9
#   0---1---2---3 /
#
FACE_COUNTS = [4, 4, 4, 3]
FACE_VERTEX_INDICES = [0, 1, 6, 5, 1, 2, 7, 6, 2, 3, 8, 7, 3, 4, 9]


def create_plane_with_subsets(stage, scheme, subsets):
    """Create the test plane with the given subdiv scheme, an st and a
    uniform primvar whose values identify the authored face, a bound
    material, and the given geom subsets.

    subsets is a list of (name, face_indices, material_name_or_None)
    tuples; material_name_or_None of None authors the subset without a
    material binding.
    """
    materials = {}
    for name in ('A', 'B', 'MeshMat'):
        materials[name] = UsdShade.Material.Define(stage, '/Mats/' + name)

    mesh = UsdGeom.Mesh.Define(stage, '/Plane')
    mesh.CreateSubdivisionSchemeAttr(scheme)

    points = [(x, y, 0) for y in (0, 1) for x in range(5)]
    mesh.CreatePointsAttr(Vt.Vec3fArray(points))
    mesh.CreateFaceVertexCountsAttr(Vt.IntArray(FACE_COUNTS))
    mesh.CreateFaceVertexIndicesAttr(Vt.IntArray(FACE_VERTEX_INDICES))

    primvars = UsdGeom.PrimvarsAPI(mesh)

    # One uniform value per face: 10 * (face + 1).
    uniform_primvar = primvars.CreatePrimvar(
        'myUniform', Sdf.ValueTypeNames.FloatArray, UsdGeom.Tokens.uniform)
    uniform_primvar.Set(
        Vt.FloatArray([10.0 * (f + 1) for f in range(len(FACE_COUNTS))]))

    # One st value per authored corner whose integer part is the authored
    # face index. Values within a face span [face, face + 0.3], so any
    # value interpolated within the face still floors to the face index.
    st = []
    for face, count in enumerate(FACE_COUNTS):
        st.extend((face + 0.1 * corner, 0.0) for corner in range(count))
    st_primvar = primvars.CreatePrimvar(
        'st', Sdf.ValueTypeNames.TexCoord2fArray, UsdGeom.Tokens.faceVarying)
    st_primvar.Set(Vt.Vec2fArray(st))

    UsdShade.MaterialBindingAPI.Apply(mesh.GetPrim()).Bind(
        materials['MeshMat'])

    for name, faces, material_name in subsets:
        subset = UsdGeom.Subset.CreateGeomSubset(
            mesh, name, UsdGeom.Tokens.face, Vt.IntArray(faces),
            'materialBind')
        if material_name is not None:
            UsdShade.MaterialBindingAPI.Apply(subset.GetPrim()).Bind(
                materials[material_name])

    return mesh


def render_and_extract(stage, refine_level=0):
    """Render the stage and return (plain, deindexed, welded) copies."""
    settings = HydraPassthrough.RenderSettings()
    settings.refineLevel = refine_level
    manager = HydraPassthrough.RenderManager()
    manager.Initialize(settings)
    try:
        manager.Render(stage)
        render_data = manager.GetRenderData()
        return (render_data.ExtractRenderDataCopy(),
                render_data.ExtractDeindexedRenderDataCopy(),
                render_data.ExtractWeldedRenderDataCopy())
    finally:
        manager.Cleanup()


def plane_path():
    prefix = HydraPassthrough.RenderManager.GetSceneDelegateId()
    return Sdf.Path(prefix.AppendPath('Plane'))


def decode_faces(prim_params):
    """primitiveParam entries -> authored face per fine primitive."""
    faces = []
    for entry in prim_params:
        encoded = entry if isinstance(entry, int) else entry[0]
        faces.append(encoded >> 2)
    return faces


class GeomSubsetsTestBase(unittest.TestCase):
    def checkDrawGroups(self, mesh, expected_groups, prims_per_face,
                        corners_per_prim):
        """Check that the mesh's draw groups tile the index buffer in
        order, bind the expected materials, have the sizes implied by
        their faces, and contain only primitives of their faces.

        expected_groups is a list of (material_suffix, face_indices).
        """
        groups = mesh.GetDrawGroups()
        self.assertEqual(len(groups), len(expected_groups))

        num_corners = len(mesh.GetFaceVertexIndices().GetValue())
        cursor = 0
        for group in groups:
            self.assertEqual(group.start, cursor)
            self.assertGreater(group.count, 0)
            cursor += group.count
        self.assertEqual(cursor, num_corners)

        faces_per_prim = decode_faces(mesh.GetPrimitiveParams().GetValue())
        self.assertEqual(len(faces_per_prim) * corners_per_prim, num_corners)

        for group, (material_suffix, faces) in zip(groups, expected_groups):
            self.assertTrue(
                str(group.materialId).endswith(material_suffix),
                f'group material {group.materialId} does not end with '
                f'{material_suffix}')
            expected_count = corners_per_prim * sum(
                prims_per_face[face] for face in faces)
            self.assertEqual(group.count, expected_count)

            first_prim = group.start // corners_per_prim
            end_prim = (group.start + group.count) // corners_per_prim
            for prim in range(first_prim, end_prim):
                self.assertIn(faces_per_prim[prim], faces)

        return groups

    def checkCornerValues(self, mesh, groups, expected_groups):
        """In a single-index layout (deindexed or welded), st and
        myUniform must resolve to a face of the group at every corner."""
        indices = mesh.GetFaceVertexIndices().GetValue()
        st = mesh.GetPrimvar('st').data.GetValue()
        uniform = mesh.GetPrimvar('myUniform').data.GetValue()
        for group, (_, faces) in zip(groups, expected_groups):
            for corner in range(group.start, group.start + group.count):
                st_face = math.floor(st[indices[corner]][0] + 1e-4)
                self.assertIn(st_face, faces)
                uniform_face = int(uniform[indices[corner]] / 10.0 - 1)
                self.assertIn(uniform_face, faces)


class TestGeomSubsets(GeomSubsetsTestBase):
    # Subset A lists its faces out of order on purpose; face 1 is in no
    # subset and falls to the remainder group.
    SUBSETS = [('subsetA', [3, 0], 'A'), ('subsetB', [2], 'B')]
    EXPECTED_GROUPS = [('Mats/A', [3, 0]), ('Mats/B', [2]),
                       ('Mats/MeshMat', [1])]

    def runCase(self, scheme, refine_level, prims_per_face,
                corners_per_prim):
        stage = Usd.Stage.CreateInMemory()
        create_plane_with_subsets(stage, scheme, self.SUBSETS)
        plain, deindexed, welded = render_and_extract(stage, refine_level)

        # The subset materials arrived through normal material sync.
        prefix = HydraPassthrough.RenderManager.GetSceneDelegateId()
        for name in ('A', 'B', 'MeshMat'):
            self.assertIsNotNone(
                plain.GetMaterial(Sdf.Path(prefix.AppendPath('Mats/' + name))))

        # The sync-time subset record round-trips, sanitized but in
        # authored order.
        mesh = plain.GetMesh(plane_path())
        subsets = mesh.GetGeomSubsets()
        self.assertEqual(len(subsets), 2)
        self.assertTrue(str(subsets[0].id).endswith('subsetA'))
        self.assertEqual(list(subsets[0].GetFaceIndices().GetValue()), [3, 0])
        self.assertTrue(str(subsets[1].materialId).endswith('Mats/B'))

        for copy in (plain, deindexed, welded):
            mesh = copy.GetMesh(plane_path())
            groups = self.checkDrawGroups(
                mesh, self.EXPECTED_GROUPS, prims_per_face, corners_per_prim)
            if copy is not plain:
                self.checkCornerValues(mesh, groups, self.EXPECTED_GROUPS)

    def test_Unrefined(self):
        # Quads triangulate to two triangles, the triangle stays one;
        # primitiveParam has one entry per triangle.
        self.runCase(UsdGeom.Tokens.none, 0,
                     prims_per_face=[2, 2, 2, 1], corners_per_prim=3)

    def test_Refined(self):
        # Catmull-clark level 1 refines a quad into four patches and the
        # triangle into three; each patch is a tri-quad of six corners.
        self.runCase(UsdGeom.Tokens.catmullClark, 1,
                     prims_per_face=[4, 4, 4, 3], corners_per_prim=6)


class TestGeomSubsetEdgeCases(GeomSubsetsTestBase):
    PRIMS_PER_FACE = [2, 2, 2, 1]  # unrefined, used by all cases below

    def extractPlane(self, subsets):
        stage = Usd.Stage.CreateInMemory()
        create_plane_with_subsets(stage, UsdGeom.Tokens.none, subsets)
        plain, _, _ = render_and_extract(stage)
        return plain.GetMesh(plane_path())

    def test_NoSubsets(self):
        mesh = self.extractPlane([])
        self.assertEqual(mesh.GetGeomSubsets(), [])
        self.assertEqual(mesh.GetDrawGroups(), [])
        # And the mesh is untouched: triangulated in authored face order.
        faces = decode_faces(mesh.GetPrimitiveParams().GetValue())
        self.assertEqual(faces, sorted(faces))

    def test_FullCoverage(self):
        # Subsets cover every face, so there is no remainder group.
        mesh = self.extractPlane(
            [('subsetA', [1, 3], 'A'), ('subsetB', [0, 2], 'B')])
        self.checkDrawGroups(
            mesh, [('Mats/A', [1, 3]), ('Mats/B', [0, 2])],
            self.PRIMS_PER_FACE, corners_per_prim=3)

    def test_OutOfRangeFaceDropped(self):
        # Sanitizing warns about face 17 and drops it from the subset;
        # the valid face still forms its group.
        mesh = self.extractPlane([('subsetA', [17, 2], 'A')])
        subsets = mesh.GetGeomSubsets()
        self.assertEqual(len(subsets), 1)
        self.assertEqual(list(subsets[0].GetFaceIndices().GetValue()), [2])
        self.checkDrawGroups(
            mesh, [('Mats/A', [2]), ('Mats/MeshMat', [0, 1, 3])],
            self.PRIMS_PER_FACE, corners_per_prim=3)

    def test_DuplicateFaceFirstSubsetWins(self):
        # Face 2 appears in both subsets; sanitizing warns but keeps it,
        # and grouping assigns it to the first subset that lists it.
        mesh = self.extractPlane(
            [('subsetA', [2, 0], 'A'), ('subsetB', [2, 3], 'B')])
        self.checkDrawGroups(
            mesh, [('Mats/A', [2, 0]), ('Mats/B', [3]),
                   ('Mats/MeshMat', [1])],
            self.PRIMS_PER_FACE, corners_per_prim=3)

    def test_SubsetWithoutOwnMaterial(self):
        # A subset with no binding of its own inherits the mesh's material
        # through normal binding resolution, so it is not dropped by
        # sanitizing: it survives with the inherited material and forms
        # its own group, bound the same as the remainder.
        mesh = self.extractPlane(
            [('subsetA', [0], 'A'), ('subsetNoMat', [2], None)])
        subsets = mesh.GetGeomSubsets()
        self.assertEqual(len(subsets), 2)
        self.assertTrue(str(subsets[1].id).endswith('subsetNoMat'))
        self.assertTrue(str(subsets[1].materialId).endswith('Mats/MeshMat'))
        self.checkDrawGroups(
            mesh, [('Mats/A', [0]), ('Mats/MeshMat', [2]),
                   ('Mats/MeshMat', [1, 3])],
            self.PRIMS_PER_FACE, corners_per_prim=3)


if __name__ == '__main__':
    unittest.main()
