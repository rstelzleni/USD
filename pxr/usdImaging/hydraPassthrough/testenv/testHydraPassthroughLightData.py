#!/pxrpythonsubst

from pxr import Gf, HydraPassthrough, Sdf, Usd, UsdGeom, UsdLux
import unittest


class TestLightData(unittest.TestCase):
    def test_Basic(self):
        stage = Usd.Stage.CreateInMemory()
        world = UsdGeom.Xform.Define(stage, '/World')
        UsdGeom.XformCommonAPI(world).SetTranslate((0, 0, 10))

        sphere = UsdLux.SphereLight.Define(stage, '/World/Sphere')
        sphere.CreateRadiusAttr(2.0)
        sphere.CreateIntensityAttr(3.0)
        sphere.CreateExposureAttr(1.5)
        sphere.CreateColorAttr(Gf.Vec3f(1, 0, 0))
        sphere.CreateTreatAsPointAttr(True)
        UsdGeom.XformCommonAPI(sphere).SetTranslate((1, 2, 3))
        shadow = UsdLux.ShadowAPI.Apply(sphere.GetPrim())
        shadow.CreateShadowEnableAttr(False)
        shadow.CreateShadowColorAttr(Gf.Vec3f(0, 0, 1))

        rect = UsdLux.RectLight.Define(stage, '/World/Rect')
        rect.CreateWidthAttr(4.0)
        rect.CreateHeightAttr(2.0)
        rect.CreateEnableColorTemperatureAttr(True)
        rect.CreateColorTemperatureAttr(3000.0)

        disk = UsdLux.DiskLight.Define(stage, '/World/Disk')
        disk.CreateRadiusAttr(3.0)
        shaping = UsdLux.ShapingAPI.Apply(disk.GetPrim())
        shaping.CreateShapingConeAngleAttr(45.0)
        shaping.CreateShapingConeSoftnessAttr(0.25)
        shaping.CreateShapingFocusAttr(2.0)

        cyl = UsdLux.CylinderLight.Define(stage, '/World/Cylinder')
        cyl.CreateRadiusAttr(0.25)
        cyl.CreateLengthAttr(5.0)
        cyl.CreateTreatAsLineAttr(True)
        UsdGeom.Imageable(cyl.GetPrim()).MakeInvisible()

        distant = UsdLux.DistantLight.Define(stage, '/World/Distant')
        distant.CreateAngleAttr(0.75)

        dome = UsdLux.DomeLight.Define(stage, '/World/Dome')
        dome.CreateTextureFileAttr('/no/such/env.exr')
        dome.CreateTextureFormatAttr('latlong')

        m = HydraPassthrough.RenderManager()
        m.Initialize()
        m.Render(stage)

        md = m.GetRenderData()
        prefix = HydraPassthrough.RenderManager.GetSceneDelegateId()

        self.assertEqual(md.GetLightCount(), 6)

        LightType = HydraPassthrough.RenderData.LightType

        # Sphere light, including shadow params and transform composition
        light = md.GetLight(prefix.AppendPath('World/Sphere'))
        self.assertIsNotNone(light)
        self.assertEqual(light.type, LightType.Sphere)
        self.assertTrue(light.visible)
        self.assertEqual(light.transform,
                         Gf.Matrix4d().SetTranslate(Gf.Vec3d(1, 2, 13)))
        self.assertEqual(light.radius, 2.0)
        self.assertEqual(light.intensity, 3.0)
        self.assertEqual(light.exposure, 1.5)
        self.assertEqual(light.color, Gf.Vec3f(1, 0, 0))
        self.assertEqual(light.treatAsPoint, True)
        self.assertEqual(light.shadowEnable, False)
        self.assertEqual(light.shadowColor, Gf.Vec3f(0, 0, 1))
        # Unauthored params come through as schema defaults
        self.assertEqual(light.normalize, False)
        self.assertEqual(light.diffuse, 1.0)
        self.assertEqual(light.specular, 1.0)
        self.assertEqual(light.shapingConeAngle, 90.0)

        # Rect light with color temperature
        light = md.GetLight(prefix.AppendPath('World/Rect'))
        self.assertIsNotNone(light)
        self.assertEqual(light.type, LightType.Rect)
        self.assertEqual(light.width, 4.0)
        self.assertEqual(light.height, 2.0)
        self.assertEqual(light.enableColorTemperature, True)
        self.assertEqual(light.colorTemperature, 3000.0)

        # Disk light with shaping
        light = md.GetLight(prefix.AppendPath('World/Disk'))
        self.assertIsNotNone(light)
        self.assertEqual(light.type, LightType.Disk)
        self.assertEqual(light.radius, 3.0)
        self.assertEqual(light.shapingConeAngle, 45.0)
        self.assertEqual(light.shapingConeSoftness, 0.25)
        self.assertEqual(light.shapingFocus, 2.0)

        # Invisible cylinder light is still reported, marked invisible
        light = md.GetLight(prefix.AppendPath('World/Cylinder'))
        self.assertIsNotNone(light)
        self.assertEqual(light.type, LightType.Cylinder)
        self.assertFalse(light.visible)
        self.assertEqual(light.radius, 0.25)
        self.assertEqual(light.length, 5.0)
        self.assertEqual(light.treatAsLine, True)

        # Distant light. The intensity assert proves schema fallbacks come
        # through: DistantLight overrides the LightAPI default with a
        # sun-like intensity.
        light = md.GetLight(prefix.AppendPath('World/Distant'))
        self.assertIsNotNone(light)
        self.assertEqual(light.type, LightType.Distant)
        self.assertEqual(light.angle, 0.75)
        self.assertEqual(light.intensity, 50000.0)

        # Dome light with texture
        light = md.GetLight(prefix.AppendPath('World/Dome'))
        self.assertIsNotNone(light)
        self.assertEqual(light.type, LightType.Dome)
        self.assertEqual(light.textureFile, '/no/such/env.exr')
        self.assertEqual(light.GetTextureFormat(), 'latlong')

        # Lights survive into the extracted copies
        rd = md.ExtractRenderDataCopy()
        self.assertEqual(rd.GetLightCount(), 6)
        light = rd.GetLight(prefix.AppendPath('World/Sphere'))
        self.assertIsNotNone(light)
        self.assertEqual(light.type, LightType.Sphere)

        m.Cleanup()

    def test_LightsOnlyStage(self):
        # A stage with no geometry still reports its lights
        stage = Usd.Stage.CreateInMemory()
        UsdLux.SphereLight.Define(stage, '/Light')

        m = HydraPassthrough.RenderManager()
        m.Initialize()
        m.Render(stage)

        md = m.GetRenderData()
        prefix = HydraPassthrough.RenderManager.GetSceneDelegateId()

        self.assertEqual(md.GetLightCount(), 1)
        light = md.GetLightByIndex(0)
        self.assertEqual(light.id, Sdf.Path(prefix.AppendPath('Light')))
        self.assertEqual(light.intensity, 1.0)
        self.assertEqual(light.transform, Gf.Matrix4d(1))

        m.Cleanup()

if __name__ == "__main__":
    unittest.main()
