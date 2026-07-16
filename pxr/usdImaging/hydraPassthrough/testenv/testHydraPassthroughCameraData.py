#!/pxrpythonsubst

from pxr import Gf, HydraPassthrough, Sdf, Usd, UsdGeom
import unittest


class TestCameraData(unittest.TestCase):
    def test_Basic(self):
        stage = Usd.Stage.CreateInMemory()
        cam = UsdGeom.Camera.Define(stage, '/Camera')
        m = HydraPassthrough.RenderManager()
        m.Initialize()
        m.Render(stage)

        md = m.GetRenderData()

        prefix = HydraPassthrough.RenderManager.GetSceneDelegateId()

        self.assertEqual(md.GetCameraCount(), 1)
        cam = md.GetCameraByIndex(0)
        self.assertEqual(cam.id, Sdf.Path(prefix.AppendPath('Camera')))

        # Make sure all the getters work and translate to python
        x = cam.transform
        self.assertEqual(x, Gf.Matrix4d(1))
        x = cam.projectionMatrix
        self.assertTrue(isinstance(x, Gf.Matrix4d))
        self.assertNotEqual(x, Gf.Matrix4d(1))
        x = cam.projection
        self.assertEqual(x, HydraPassthrough.RenderData.Projection.Perspective)
        x = cam.clippingRange
        self.assertLessEqual(x.min, 1.0)
        self.assertGreaterEqual(x.max, 1000.0)
        cp = cam.GetClipPlanes()
        self.assertEqual(len(cp), 0)
        x = cam.focalLength
        self.assertGreaterEqual(x, 5.0)
        x = cam.horizontalAperture
        self.assertLess(x, 3.0)
        x = cam.verticalAperture
        self.assertLess(x, 2.0)
        x = cam.horizontalApertureOffset
        self.assertEqual(x, 0.0)
        x = cam.verticalApertureOffset
        self.assertEqual(x, 0.0)
        x = cam.fStop
        self.assertEqual(x, 0.0)
        x = cam.focusDistance
        self.assertEqual(x, 0.0)
        x = cam.focusOn
        self.assertEqual(x, False)
        x = cam.dofAspect
        self.assertEqual(x, 1.0)
        x = cam.splitDiopterCount
        self.assertEqual(x, 0)
        x = cam.splitDiopterAngle
        self.assertEqual(x, 0.0)
        x = cam.splitDiopterOffset1
        self.assertEqual(x, 0.0)
        x = cam.splitDiopterWidth1
        self.assertEqual(x, 0.0)
        x = cam.splitDiopterFocusDistance1
        self.assertEqual(x, 0.0)
        x = cam.splitDiopterOffset2
        self.assertEqual(x, 0.0)
        x = cam.splitDiopterWidth2
        self.assertEqual(x, 0.0)
        x = cam.splitDiopterFocusDistance2
        self.assertEqual(x, 0.0)
        x = cam.shutterOpen
        self.assertEqual(x, 0.0)
        x = cam.shutterClose
        self.assertEqual(x, 0.0)
        x = cam.linearExposureScale
        self.assertEqual(x, 1.0)
        x = cam.GetLensDistortionType()
        self.assertEqual(x, "standard")
        x = cam.lensDistortionK1
        self.assertEqual(x, 0.0)
        x = cam.lensDistortionK2
        self.assertEqual(x, 0.0)
        x = cam.lensDistortionCenter
        self.assertEqual(x, Gf.Vec2f(0.0))
        x = cam.lensDistortionAnaSq
        self.assertEqual(x, 1.0)
        x = cam.lensDistortionAsym
        self.assertEqual(x, Gf.Vec2f(0.0))
        x = cam.lensDistortionScale
        self.assertEqual(x, 1.0)
        x = cam.lensDistortionIor
        self.assertEqual(x, 0.0)
        x = cam.windowPolicy
        self.assertEqual(x, HydraPassthrough.RenderData.WindowPolicy.Fit)

        m.Cleanup()

    def test_CameraUnderRenderTaskScope(self):
        # Scene cameras are identified by the scene path prefix, so a
        # camera under a prim that happens to be named RenderTask must
        # still come through (it used to be dropped by a substring match
        # meant for the internal render task camera).
        stage = Usd.Stage.CreateInMemory()
        UsdGeom.Camera.Define(stage, '/RenderTask/Camera')
        m = HydraPassthrough.RenderManager()
        m.Initialize()
        m.Render(stage)

        md = m.GetRenderData()
        prefix = HydraPassthrough.RenderManager.GetSceneDelegateId()

        self.assertEqual(md.GetCameraCount(), 1)
        cam = md.GetCameraByIndex(0)
        self.assertEqual(cam.id,
                         Sdf.Path(prefix.AppendPath('RenderTask/Camera')))

        m.Cleanup()

if __name__ == "__main__":
    unittest.main()
