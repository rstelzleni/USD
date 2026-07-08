#!/pxrpythonsubst

from pxr import Gf, HydraPassthrough, Sdf, Usd, UsdGeom, UsdSkel, Vt
import unittest


def create_skinned_quad_usd(stage):
    """Create a minimal UsdSkel setup: a single quad whose bottom edge is
    bound to a root joint and whose top edge is bound to a child joint that
    the animation translates +1 in Y from its bind position.

    All animation values are authored as defaults (no time samples) so the
    result is the same at whatever time the render samples the stage.
    """
    skelRoot = UsdSkel.Root.Define(stage, '/SkelRoot')

    # Two-joint chain along +Y. Rest pose matches the bind pose.
    skel = UsdSkel.Skeleton.Define(stage, '/SkelRoot/Skel')
    skel.CreateJointsAttr().Set(Vt.TokenArray(['root', 'root/tip']))
    tipBind = Gf.Matrix4d(1).SetTranslate(Gf.Vec3d(0, 1, 0))
    skel.CreateBindTransformsAttr().Set(
        Vt.Matrix4dArray([Gf.Matrix4d(1), tipBind]))
    skel.CreateRestTransformsAttr().Set(
        Vt.Matrix4dArray([Gf.Matrix4d(1), tipBind]))

    # Animation moves the tip joint to local (0, 2, 0): +1 Y from bind.
    anim = UsdSkel.Animation.Define(stage, '/SkelRoot/Anim')
    anim.CreateJointsAttr().Set(Vt.TokenArray(['root/tip']))
    anim.CreateTranslationsAttr().Set(Vt.Vec3fArray([Gf.Vec3f(0, 2, 0)]))
    anim.CreateRotationsAttr().Set(Vt.QuatfArray([Gf.Quatf(1)]))
    anim.CreateScalesAttr().Set(Vt.Vec3hArray([Gf.Vec3h(1, 1, 1)]))
    UsdSkel.BindingAPI.Apply(skel.GetPrim()).CreateAnimationSourceRel() \
        .SetTargets([anim.GetPath()])

    # Unit quad in the XY plane. Bottom points follow root, top points
    # follow tip, one influence per point.
    mesh = UsdGeom.Mesh.Define(stage, '/SkelRoot/Mesh')
    mesh.CreatePointsAttr().Set(Vt.Vec3fArray([
        Gf.Vec3f(0, 0, 0), Gf.Vec3f(1, 0, 0),
        Gf.Vec3f(0, 1, 0), Gf.Vec3f(1, 1, 0)]))
    mesh.CreateFaceVertexCountsAttr().Set(Vt.IntArray([4]))
    mesh.CreateFaceVertexIndicesAttr().Set(Vt.IntArray([0, 1, 3, 2]))
    # A non-skel primvar that must survive the skel primvar filtering.
    UsdGeom.PrimvarsAPI(mesh.GetPrim()).CreatePrimvar(
        'displayColor', Sdf.ValueTypeNames.Color3fArray,
        UsdGeom.Tokens.constant).Set(Vt.Vec3fArray([Gf.Vec3f(1, 0, 0)]))

    binding = UsdSkel.BindingAPI.Apply(mesh.GetPrim())
    binding.CreateSkeletonRel().SetTargets([skel.GetPath()])
    binding.CreateJointIndicesPrimvar(constant=False, elementSize=1).Set(
        Vt.IntArray([0, 0, 1, 1]))
    binding.CreateJointWeightsPrimvar(constant=False, elementSize=1).Set(
        Vt.FloatArray([1, 1, 1, 1]))

    return mesh


class TestSkinning(unittest.TestCase):
    def test_Skinning(self):
        stage = Usd.Stage.CreateInMemory()
        create_skinned_quad_usd(stage)

        settings = HydraPassthrough.RenderSettings()
        settings.refineLevel = 0
        m = HydraPassthrough.RenderManager()
        m.Initialize(settings)
        # Any Tf error raised while syncing (e.g. an unsupported prim type
        # for the skinning extComputations) surfaces here as an exception.
        m.Render(stage)

        md = m.GetRenderData()
        prefix = HydraPassthrough.RenderManager.GetSceneDelegateId()
        self.assertEqual(md.GetMeshCount(), 1)
        mesh = md.GetMesh(Sdf.Path(prefix.AppendPath('SkelRoot/Mesh')))

        # The skinning computation replaces the authored points: the bottom
        # edge is bound to the unmoved root, the top edge to the tip joint,
        # which the animation moved +1 in Y from its bind position.
        expected = [
            Gf.Vec3f(0, 0, 0), Gf.Vec3f(1, 0, 0),
            Gf.Vec3f(0, 2, 0), Gf.Vec3f(1, 2, 0)]
        points = mesh.GetPoints().GetValue()
        self.assertEqual(len(points), len(expected))
        for i, (actual, exp) in enumerate(zip(points, expected)):
            self.assertTrue(Gf.IsClose(Gf.Vec3f(actual), exp, 1e-5),
                            'point %d: %s != %s' % (i, actual, exp))

        # Skinning inputs are consumed by the computation and must not leak
        # to clients, while other primvars pass through untouched.
        primvarNames = [pv.name for pv in mesh.GetAllPrimvars()]
        self.assertFalse(
            [name for name in primvarNames if name.startswith('skel:')],
            'skel primvars leaked: %s' % primvarNames)
        self.assertIn('displayColor', primvarNames)

        m.Cleanup()


if __name__ == "__main__":
    unittest.main()
