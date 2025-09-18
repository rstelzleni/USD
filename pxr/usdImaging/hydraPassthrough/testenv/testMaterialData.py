
#!/pxrpythonsubst

from pxr import Gf, HydraPassthrough, Sdf, Usd, UsdGeom, UsdShade
import unittest


class TestMaterialData(unittest.TestCase):
    def test_Basic(self):
        stage = Usd.Stage.CreateInMemory()
        mat = UsdShade.Material.Define(stage, '/Material0')
        mat = UsdShade.Material.Define(stage, '/Material1')
        mat = UsdShade.Material.Define(stage, '/Material2')
        m = HydraPassthrough.RenderManager()
        m.Initialize()
        m.Render(stage)

        md = m.GetRenderData()

        prefix = md.GetSceneDelegateId()

        self.assertEqual(md.GetMaterialCount(), 3)
        mat0 = md.GetMaterial(Sdf.Path(prefix.AppendPath('Material0')))
        self.assertEqual(mat0.id, Sdf.Path(prefix.AppendPath('Material0')))

        # Make sure all the getters work and translate to python

        m.Cleanup()

if __name__ == "__main__":
    unittest.main()
