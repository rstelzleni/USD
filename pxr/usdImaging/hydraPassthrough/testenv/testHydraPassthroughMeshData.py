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



if __name__ == "__main__":
    unittest.main()
