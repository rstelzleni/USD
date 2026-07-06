#!/pxrpythonsubst

from pxr import Gf, HydraPassthrough, Sdf, Usd, UsdGeom, Vt
import unittest


def render_extract(stage):
    """Render the stage and return an extracted RenderData copy."""
    m = HydraPassthrough.RenderManager()
    m.Initialize()
    try:
        m.Render(stage)
        return m.GetRenderData().ExtractRenderDataCopy()
    finally:
        m.Cleanup()


def get_meshes(data):
    return [data.GetMeshByIndex(i) for i in range(data.GetMeshCount())]


def create_quad(stage, path):
    """A single unsubdivided quad to use as an instancing prototype."""
    mesh = UsdGeom.Mesh.Define(stage, path)
    mesh.CreateSubdivisionSchemeAttr(UsdGeom.Tokens.none)
    mesh.CreatePointsAttr(
        Vt.Vec3fArray([(0, 0, 0), (1, 0, 0), (1, 1, 0), (0, 1, 0)]))
    mesh.CreateFaceVertexCountsAttr(Vt.IntArray([4]))
    mesh.CreateFaceVertexIndicesAttr(Vt.IntArray([0, 1, 2, 3]))
    return mesh


def translations(xforms, ndigits=3):
    """Sorted, rounded translation tuples for order-insensitive compares."""
    return sorted(
        tuple(round(c, ndigits) for c in x.ExtractTranslation())
        for x in xforms)


class TestInstancing(unittest.TestCase):

    def assertMatrixClose(self, a, b, eps=1e-4):
        for row in range(4):
            ra = a.GetRow(row)
            rb = b.GetRow(row)
            for col in range(4):
                self.assertAlmostEqual(
                    ra[col], rb[col], delta=eps,
                    msg='mismatch at [%d][%d]:\n%s\nvs\n%s' % (
                        row, col, a, b))

    def test_NonInstancedMesh(self):
        # A plain mesh reports no instancer and no instance transforms
        stage = Usd.Stage.CreateInMemory()
        create_quad(stage, '/Quad')

        data = render_extract(stage)
        meshes = get_meshes(data)
        self.assertEqual(len(meshes), 1)
        self.assertTrue(meshes[0].instancerId.isEmpty)
        self.assertFalse(meshes[0].GetInstanceTransforms().GetValue())

    def test_PointInstancer(self):
        # A point instancer with positions, orientations and scales, and a
        # transform on the instancer itself. The exported transforms must
        # match what UsdGeomPointInstancer computes, composed with the
        # instancer's own local-to-world.
        stage = Usd.Stage.CreateInMemory()
        pi = UsdGeom.PointInstancer.Define(stage, '/PI')
        pi.AddTranslateOp().Set(Gf.Vec3d(10, 0, 0))
        create_quad(stage, '/PI/Protos/Quad')
        pi.CreatePrototypesRel().SetTargets([Sdf.Path('/PI/Protos/Quad')])
        pi.CreateProtoIndicesAttr(Vt.IntArray([0, 0, 0]))
        pi.CreatePositionsAttr(
            Vt.Vec3fArray([(0, 0, 0), (5, 0, 0), (0, 5, 0)]))
        pi.CreateScalesAttr(
            Vt.Vec3fArray([(1, 1, 1), (2, 2, 2), (1, 3, 1)]))
        # 90 degrees about Z for the middle instance
        pi.CreateOrientationsAttr(Vt.QuathArray([
            Gf.Quath(1),
            Gf.Quath(0.7071, 0, 0, 0.7071),
            Gf.Quath(1)]))

        # Expected: the usd-computed per-instance transforms (in instancer
        # space) times the instancer's local-to-world. Computed before
        # rendering because RenderManager.Cleanup currently expires the
        # caller's stage handles.
        instancer_l2w = UsdGeom.XformCache().GetLocalToWorldTransform(
            pi.GetPrim())
        expected = pi.ComputeInstanceTransformsAtTime(
            Usd.TimeCode.Default(), Usd.TimeCode.Default(),
            UsdGeom.PointInstancer.ExcludeProtoXform)
        self.assertEqual(len(expected), 3)

        data = render_extract(stage)
        meshes = get_meshes(data)
        self.assertEqual(len(meshes), 1)
        mesh = meshes[0]

        self.assertFalse(mesh.instancerId.isEmpty)
        xforms = mesh.GetInstanceTransforms().GetValue()
        self.assertEqual(len(xforms), 3)

        # A single-prototype instancer preserves point order
        for got, want in zip(xforms, expected):
            self.assertMatrixClose(got, want * instancer_l2w)

    def test_PointInstancerMultiplePrototypes(self):
        # Two prototypes sharing one instancer: each propagated mesh gets
        # the transforms for its own protoIndices entries.
        stage = Usd.Stage.CreateInMemory()
        pi = UsdGeom.PointInstancer.Define(stage, '/PI')
        create_quad(stage, '/PI/Protos/QuadA')
        create_quad(stage, '/PI/Protos/QuadB')
        pi.CreatePrototypesRel().SetTargets(
            [Sdf.Path('/PI/Protos/QuadA'), Sdf.Path('/PI/Protos/QuadB')])
        pi.CreateProtoIndicesAttr(Vt.IntArray([0, 1, 0]))
        pi.CreatePositionsAttr(
            Vt.Vec3fArray([(0, 0, 0), (5, 0, 0), (0, 5, 0)]))

        data = render_extract(stage)
        meshes = get_meshes(data)
        self.assertEqual(len(meshes), 2)

        counts = {}
        for mesh in meshes:
            self.assertFalse(mesh.instancerId.isEmpty)
            xforms = mesh.GetInstanceTransforms().GetValue()
            counts[len(xforms)] = translations(xforms)

        # QuadA is instanced at points 0 and 2, QuadB at point 1
        self.assertEqual(set(counts.keys()), {1, 2})
        self.assertEqual(counts[2], [(0, 0, 0), (0, 5, 0)])
        self.assertEqual(counts[1], [(5, 0, 0)])

    def test_PointInstancerInvisibleIds(self):
        # Masked instances are dropped from the transform list without
        # renumbering the surviving ones.
        stage = Usd.Stage.CreateInMemory()
        pi = UsdGeom.PointInstancer.Define(stage, '/PI')
        create_quad(stage, '/PI/Protos/Quad')
        pi.CreatePrototypesRel().SetTargets([Sdf.Path('/PI/Protos/Quad')])
        pi.CreateProtoIndicesAttr(Vt.IntArray([0, 0, 0]))
        pi.CreatePositionsAttr(
            Vt.Vec3fArray([(0, 0, 0), (5, 0, 0), (0, 5, 0)]))
        pi.CreateInvisibleIdsAttr(Vt.Int64Array([1]))

        data = render_extract(stage)
        meshes = get_meshes(data)
        self.assertEqual(len(meshes), 1)
        mesh = meshes[0]

        self.assertFalse(mesh.instancerId.isEmpty)
        xforms = mesh.GetInstanceTransforms().GetValue()
        self.assertEqual(translations(xforms), [(0, 0, 0), (0, 5, 0)])

    def test_NativeInstancing(self):
        # Two instanceable references to one prototype aggregate into a
        # single mesh with two instance transforms.
        stage = Usd.Stage.CreateInMemory()
        stage.CreateClassPrim('/_Proto')
        create_quad(stage, '/_Proto/Geom')

        offsets = [(0, 0, 0), (5, 0, 0)]
        for i, offset in enumerate(offsets):
            xf = UsdGeom.Xform.Define(stage, '/Instance%d' % i)
            xf.AddTranslateOp().Set(Gf.Vec3d(*offset))
            prim = xf.GetPrim()
            prim.GetReferences().AddInternalReference('/_Proto')
            prim.SetInstanceable(True)

        data = render_extract(stage)
        meshes = get_meshes(data)
        self.assertEqual(len(meshes), 1)
        mesh = meshes[0]

        self.assertFalse(mesh.instancerId.isEmpty)
        xforms = mesh.GetInstanceTransforms().GetValue()
        self.assertEqual(len(xforms), 2)
        self.assertEqual(translations(xforms), sorted(offsets))

    def test_NestedPointInstancers(self):
        # A point instancer whose prototype contains another point
        # instancer: the flattened transform list covers the cross product
        # of both levels.
        stage = Usd.Stage.CreateInMemory()
        outer = UsdGeom.PointInstancer.Define(stage, '/Outer')
        inner = UsdGeom.PointInstancer.Define(stage, '/Outer/Protos/Inner')
        create_quad(stage, '/Outer/Protos/Inner/Protos/Quad')

        inner.CreatePrototypesRel().SetTargets(
            [Sdf.Path('/Outer/Protos/Inner/Protos/Quad')])
        inner.CreateProtoIndicesAttr(Vt.IntArray([0, 0]))
        inner_offsets = [(0, 0, 0), (1, 0, 0)]
        inner.CreatePositionsAttr(Vt.Vec3fArray(inner_offsets))

        outer.CreatePrototypesRel().SetTargets(
            [Sdf.Path('/Outer/Protos/Inner')])
        outer.CreateProtoIndicesAttr(Vt.IntArray([0, 0]))
        outer_offsets = [(0, 0, 0), (0, 10, 0)]
        outer.CreatePositionsAttr(Vt.Vec3fArray(outer_offsets))

        data = render_extract(stage)
        meshes = get_meshes(data)
        self.assertEqual(len(meshes), 1)
        mesh = meshes[0]

        self.assertFalse(mesh.instancerId.isEmpty)
        xforms = mesh.GetInstanceTransforms().GetValue()
        self.assertEqual(len(xforms), 4)

        expected = sorted(
            (io[0] + oo[0], io[1] + oo[1], io[2] + oo[2])
            for oo in outer_offsets for io in inner_offsets)
        self.assertEqual(translations(xforms), expected)


if __name__ == "__main__":
    unittest.main()
