
#!/pxrpythonsubst

from pxr import Gf, HydraPassthrough, Sdf, Usd, UsdGeom, UsdShade
import unittest


def populate_example_material(stage, material):
    stage = material.GetPrim().GetStage()
    preview_surface = UsdShade.Shader.Define(
        stage, material.GetPath().AppendPath("SurfaceShader")
    )
    preview_surface.CreateIdAttr("UsdPreviewSurface")
    preview_surface.CreateInput("roughness", Sdf.ValueTypeNames.Float).Set(0.7)
    preview_surface.CreateInput("metallic", Sdf.ValueTypeNames.Float).Set(0.0)
    material.CreateSurfaceOutput().ConnectToSource(
        preview_surface.ConnectableAPI(), "surface"
    )

    uv_reader = UsdShade.Shader.Define(stage, material.GetPath().AppendPath("UVReader"))
    uv_reader.CreateIdAttr("UsdPrimvarReader_float2")
    uv_reader.CreateInput("varname", Sdf.ValueTypeNames.Token).Set("uv")

    sampler = UsdShade.Shader.Define(stage, material.GetPath().AppendPath("Texture"))
    sampler.CreateIdAttr("UsdUVTexture")
    sampler.CreateInput("file", Sdf.ValueTypeNames.Asset).Set("image.png")
    sampler.CreateInput("st", Sdf.ValueTypeNames.Float2).ConnectToSource(
        uv_reader.ConnectableAPI(), "result"
    )
    sampler.CreateInput("wrapS", Sdf.ValueTypeNames.Token).Set("repeat")
    sampler.CreateOutput("rgb", Sdf.ValueTypeNames.Float3)
    preview_surface.CreateInput(
        "diffuseColor", Sdf.ValueTypeNames.Color3f
    ).ConnectToSource(sampler.ConnectableAPI(), "rgb")
    preview_surface.CreateInput("opacity", Sdf.ValueTypeNames.Float).ConnectToSource(
        sampler.ConnectableAPI(), "a"
    )


def populate_mesh(mesh):
    # hard code some billboards
    mesh.CreatePointsAttr(
        [(-1.0, 0.0, -1.0), (1.0, 0.0, -1.0), (1.0, 0.0, 1.0), (-1.0, 0.0, 1.0)]
    )
    mesh.CreateFaceVertexCountsAttr([4])
    mesh.CreateFaceVertexIndicesAttr([0, 1, 2, 3])
    tex_coords = UsdGeom.PrimvarsAPI(mesh).CreatePrimvar(
        "uv", Sdf.ValueTypeNames.TexCoord2fArray, UsdGeom.Tokens.varying
    )
    tex_coords.Set([(0.0, 0.0), (1.0, 0.0), (1.0, 1.0), (0.0, 1.0)])


def assign_material_to_mesh(material, mesh):
    UsdShade.MaterialBindingAPI.Apply(mesh.GetPrim())
    UsdShade.MaterialBindingAPI(mesh).Bind(material)


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

        prefix = HydraPassthrough.RenderManager.GetSceneDelegateId()

        self.assertEqual(md.GetMaterialCount(), 3)
        mat0 = md.GetMaterial(Sdf.Path(prefix.AppendPath('Material0')))
        self.assertEqual(mat0.id, Sdf.Path(prefix.AppendPath('Material0')))

        # Make sure all the getters work and translate to python
        self.assertEqual(mat0.type, HydraPassthrough.RenderData.MaterialType.Other)
        self.assertEqual(mat0.GetMaterialTag(), '')
        self.assertEqual(mat0.GetMaterialMetadata(), {})
        self.assertEqual(mat0.GetMaterialParams(), [])
        self.assertEqual(mat0.GetTextureDescriptors(), [])

        m.Cleanup()

    def test_Billboard(self):
        stage = Usd.Stage.CreateInMemory()
        mat = UsdShade.Material.Define(stage, '/Material')
        populate_example_material(stage, mat)

        mesh = UsdGeom.Mesh.Define(stage, '/Mesh')
        populate_mesh(mesh)
        assign_material_to_mesh(mat, mesh)

        m = HydraPassthrough.RenderManager()
        m.Initialize()
        m.Render(stage)

        md = m.GetRenderData()

        prefix = HydraPassthrough.RenderManager.GetSceneDelegateId()

        self.assertEqual(md.GetMaterialCount(), 1)
        mat0 = md.GetMaterial(Sdf.Path(prefix.AppendPath('Material')))
        self.assertEqual(mat0.id, Sdf.Path(prefix.AppendPath('Material')))

        # Make sure all the getters work and translate to python
        self.assertEqual(mat0.type, HydraPassthrough.RenderData.MaterialType.PreviewSurface)
        self.assertEqual(mat0.GetMaterialTag(), 'translucent')
        self.assertEqual(len(mat0.GetMaterialParams()), 21)
        self.assertEqual(len(mat0.GetTextureDescriptors()), 2)
        self.assertEqual(mat0.GetMaterialMetadata(), {})

        for p in mat0.GetMaterialParams():
            self.assertTrue(isinstance(p.name, str))
            self.assertTrue(isinstance(p.paramType, HydraPassthrough.MaterialParam.ParamType))
            if p.name == 'diffuseColor':
                self.assertEqual(p.paramType, HydraPassthrough.MaterialParam.ParamType.Texture)
                #self.assertEqual(p.fallbackValue, Gf.Vec3f(0,0,0))
                self.assertEqual(p.GetSamplerCoords(), ['uv'])
                self.assertEqual(p.textureType, HydraPassthrough.MaterialParam.TextureType.Uv)
                self.assertEqual(p.swizzle, 'xyz')
                self.assertEqual(p.isPremultiplied, True)
                self.assertEqual(p.arrayOfTexturesSize, 0)
            elif p.name == 'opacity':
                self.assertEqual(p.paramType, HydraPassthrough.MaterialParam.ParamType.Texture)
                #self.assertEqual(p.fallbackValue, 0)
                self.assertEqual(p.GetSamplerCoords(), ['uv'])
                self.assertEqual(p.textureType, HydraPassthrough.MaterialParam.TextureType.Uv)
                self.assertEqual(p.swizzle, 'w')
                self.assertEqual(p.isPremultiplied, False)
                self.assertEqual(p.arrayOfTexturesSize, 0)
            elif p.name == 'uv':
                self.assertEqual(p.paramType, HydraPassthrough.MaterialParam.ParamType.AdditionalPrimvar)
                #self.assertEqual(p.fallbackValue, None)
                self.assertEqual(p.GetSamplerCoords(), [])
                # This seems wrong, this isn't a uv texture, it's a primvar. This is what storm does
                # internally, so I guess we'll run with it for now.
                self.assertEqual(p.textureType, HydraPassthrough.MaterialParam.TextureType.Uv)
                self.assertEqual(p.swizzle, '')
                self.assertEqual(p.isPremultiplied, False)
                self.assertEqual(p.arrayOfTexturesSize, 0)

        for t in mat0.GetTextureDescriptors():
            if t.name == 'diffuseColor':
                self.assertEqual(t.filePath, 'image.png')
                self.assertEqual(t.type, HydraPassthrough.MaterialParam.TextureType.Uv)
                # Probably just add these to the parent type's wrapper
                self.assertEqual(t.GetWrapS(), HydraPassthrough.TextureDescriptor.Wrap.Repeat)
                self.assertEqual(t.GetWrapT(), HydraPassthrough.TextureDescriptor.Wrap.NoOpinion)
                self.assertEqual(t.GetWrapR(), HydraPassthrough.TextureDescriptor.Wrap.NoOpinion)
                self.assertEqual(t.GetMinFilter(), HydraPassthrough.TextureDescriptor.MinFilter.LinearMipmapLinear)
                self.assertEqual(t.GetMagFilter(), HydraPassthrough.TextureDescriptor.MagFilter.Linear)
                self.assertEqual(t.GetBorderColor(), HydraPassthrough.TextureDescriptor.BorderColor.TransparentBlack)
                self.assertEqual(t.GetMaxAnisotropy(), 16)
            if t.name == 'opacity':
                self.assertEqual(t.filePath, 'image.png')
                self.assertEqual(t.type, HydraPassthrough.MaterialParam.TextureType.Uv)
                self.assertEqual(t.GetWrapS(), HydraPassthrough.TextureDescriptor.Wrap.Repeat)
                self.assertEqual(t.GetWrapT(), HydraPassthrough.TextureDescriptor.Wrap.NoOpinion)
                self.assertEqual(t.GetWrapR(), HydraPassthrough.TextureDescriptor.Wrap.NoOpinion)
                self.assertEqual(t.GetMinFilter(), HydraPassthrough.TextureDescriptor.MinFilter.LinearMipmapLinear)
                self.assertEqual(t.GetMagFilter(), HydraPassthrough.TextureDescriptor.MagFilter.Linear)
                self.assertEqual(t.GetBorderColor(), HydraPassthrough.TextureDescriptor.BorderColor.TransparentBlack)
                self.assertEqual(t.GetMaxAnisotropy(), 16)

        # check on the bound mesh
        bound_mesh = md.GetMesh(Sdf.Path(prefix.AppendPath('Mesh')))
        self.assertEqual(bound_mesh.materialId, mat0.id)
        self.assertEqual(len(bound_mesh.GetAllPrimvars()), 2) # uv and points
        primvar = bound_mesh.GetPrimvar('uv')
        self.assertTrue(primvar is not None)
        self.assertEqual(primvar.name, 'uv')
        self.assertEqual(primvar.interpolation, 'varying')
        self.assertEqual(primvar.typeName, 'VtArray<GfVec2f>')
        self.assertEqual(primvar.data, [(0.0, 0.0), (1.0, 0.0), (1.0, 1.0), (0.0, 1.0)])
        self.assertEqual(primvar.role, 'textureCoordinate')
        primvar = bound_mesh.GetPrimvar('points')
        self.assertTrue(primvar is not None)
        primvar = bound_mesh.GetPrimvar('nonexistent')
        self.assertTrue(primvar is None)
                
        m.Cleanup()

if __name__ == "__main__":
    unittest.main()
