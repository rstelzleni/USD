#!/pxrpythonsubst

from pxr import HydraPassthrough, Sdf, Usd, UsdGeom
import unittest


class TestCameraData(unittest.TestCase):
    def test_Basic(self):
        stage = Usd.Stage.CreateInMemory()
        cam = UsdGeom.Camera.Define(stage, '/Camera')
        m = HydraPassthrough.RenderManager()
        m.Initialize()
        m.Render(stage)

        md = m.GetRenderData()

        prefix = '/HdPassthroughSceneDelegate'

        self.assertEqual(md.GetCameraCount(), 1)
        cam = md.GetCameraByIndex(0)
        self.assertEqual(cam.id, Sdf.Path(prefix + '/Camera'))

        m.Cleanup()

if __name__ == "__main__":
    unittest.main()
