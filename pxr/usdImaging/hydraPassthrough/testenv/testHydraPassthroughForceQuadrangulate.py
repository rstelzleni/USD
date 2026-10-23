#!/pxrpythonsubst

# Registered with HD_ENABLE_FORCE_QUADRANGULATE=1. Storm quadrangulates
# when that is set (or a ptex material is bound), but HydraPassthrough
# always triangulates, so the output should be the same as without it.

from pxr import HydraPassthrough, Sdf, Usd, UsdGeom, Vt
import unittest


class TestForceQuadrangulate(unittest.TestCase):
    def test_TriangleAndQuad(self):
        stage = Usd.Stage.CreateInMemory()
        mesh = UsdGeom.Mesh.Define(stage, '/TriQuad')
        mesh.CreateSubdivisionSchemeAttr(UsdGeom.Tokens.none)
        mesh.CreatePointsAttr(Vt.Vec3fArray(
            [(0, 0, 0), (1, 0, 0), (0, 1, 0), (2, 0, 0), (2, 1, 0)]))
        mesh.CreateFaceVertexCountsAttr(Vt.IntArray([3, 4]))
        mesh.CreateFaceVertexIndicesAttr(Vt.IntArray([0, 1, 2, 1, 3, 4, 2]))

        m = HydraPassthrough.RenderManager()
        m.Initialize()
        try:
            m.Render(stage)
            plain = m.GetRenderData().ExtractRenderDataCopy()
            welded = m.GetRenderData().ExtractWeldedRenderDataCopy()
        finally:
            m.Cleanup()

        prefix = HydraPassthrough.RenderManager.GetSceneDelegateId()
        path = Sdf.Path(prefix.AppendPath('TriQuad'))

        # One triangle plus a quad split into two triangles
        mesh = plain.GetMesh(path)
        self.assertEqual(len(mesh.GetPoints().GetValue()), 5)
        self.assertEqual(len(mesh.GetFaceVertexIndices().GetValue()), 9)

        mesh = welded.GetMesh(path)
        num_points = len(mesh.GetPoints().GetValue())
        self.assertEqual(num_points, 5)
        indices = list(mesh.GetFaceVertexIndices().GetValue())
        self.assertEqual(len(indices), 9)
        self.assertTrue(all(0 <= i < num_points for i in indices))


if __name__ == "__main__":
    unittest.main()
