#!/pxrpythonsubst

from pxr import Gf, HydraPassthrough, Sdf, Usd, UsdGeom
import unittest


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
        x = mesh0.transform
        self.assertEqual(x, Gf.Matrix4d(1))
        x = mesh0.points
        self.assertEqual(x, [])
        x = mesh0.faceVertexIndices
        self.assertEqual(x, [])
        x = mesh0.triangleOriginalFaceIndices
        self.assertEqual(x, [])
        x = mesh0.triangleEdgeIndices
        self.assertEqual(x, [])
        x = mesh0.materialId
        self.assertEqual(x, Sdf.Path())
        x = mesh0.GetAllPrimvars()
        self.assertEqual(x, [])
        x = mesh0.GetPrimvar('nonexistent')
        self.assertEqual(x, None)

        m.Cleanup()

if __name__ == "__main__":
    unittest.main()
