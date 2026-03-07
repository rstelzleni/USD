#!/pxrpythonsubst

from pxr import Gf, HydraPassthrough, Sdf, Usd, UsdGeom, Vt
import unittest
import math

def create_uv_sphere(
    num_latitude: int = 8,
    num_longitude: int = 16,
    radius: float = 1.0
) -> tuple[list, list, list, list, list, list]:
    """
    Create a basic UV sphere mesh.
    
    Returns:
        points, face_vertex_counts, face_vertex_indices,
        uvs, uv_indices, colors
    """
    points = []
    uvs = []
    uv_indices = []
    face_vertex_counts = []
    face_vertex_indices = []
    
    # Generate points (poles + latitude rings)
    # North pole
    points.append(Gf.Vec3f(0, radius, 0))
    
    # Latitude rings
    for lat in range(1, num_latitude):
        theta = math.pi * lat / num_latitude
        y = radius * math.cos(theta)
        ring_radius = radius * math.sin(theta)
        
        for lon in range(num_longitude):
            phi = 2 * math.pi * lon / num_longitude
            x = ring_radius * math.cos(phi)
            z = ring_radius * math.sin(phi)
            points.append(Gf.Vec3f(x, y, z))
    
    # South pole
    points.append(Gf.Vec3f(0, -radius, 0))
    
    # Generate UVs - one per face vertex (face-varying)
    # UVs need extra vertices at the seam and poles
    for lat in range(num_latitude + 1):
        v = lat / num_latitude
        num_lon = 1 if (lat == 0 or lat == num_latitude) else (num_longitude + 1)
        for lon in range(num_lon):
            u = lon / num_longitude
            uvs.append(Gf.Vec2f(u, 1.0 - v))
    
    # Generate faces
    # North pole cap (triangles)
    for lon in range(num_longitude):
        face_vertex_counts.append(3)
        face_vertex_indices.extend([
            0,  # north pole
            1 + lon,
            1 + (lon + 1) % num_longitude
        ])
        # UV indices for pole triangle
        uv_indices.extend([
            0,  # pole UV
            1 + lon,
            1 + lon + 1
        ])
    
    # Middle quads
    for lat in range(1, num_latitude - 1):
        ring_start = 1 + (lat - 1) * num_longitude
        next_ring_start = 1 + lat * num_longitude
        uv_ring_start = 1 + (lat - 1) * (num_longitude + 1)
        uv_next_ring_start = 1 + lat * (num_longitude + 1)
        
        for lon in range(num_longitude):
            face_vertex_counts.append(4)
            face_vertex_indices.extend([
                ring_start + lon,
                next_ring_start + lon,
                next_ring_start + (lon + 1) % num_longitude,
                ring_start + (lon + 1) % num_longitude
            ])
            uv_indices.extend([
                uv_ring_start + lon,
                uv_next_ring_start + lon,
                uv_next_ring_start + lon + 1,
                uv_ring_start + lon + 1
            ])
    
    # South pole cap (triangles)
    south_pole_idx = len(points) - 1
    last_ring_start = 1 + (num_latitude - 2) * num_longitude
    uv_last_ring_start = 1 + (num_latitude - 2) * (num_longitude + 1)
    uv_south_pole = len(uvs) - 1
    
    for lon in range(num_longitude):
        face_vertex_counts.append(3)
        face_vertex_indices.extend([
            last_ring_start + lon,
            south_pole_idx,
            last_ring_start + (lon + 1) % num_longitude
        ])
        uv_indices.extend([
            uv_last_ring_start + lon,
            uv_south_pole,
            uv_last_ring_start + lon + 1
        ])
    
    # Generate vertex colors based on position (vertex-interpolated)
    colors = []
    for p in points:
        # Map position to RGB
        r = (p[0] / radius + 1) * 0.5
        g = (p[1] / radius + 1) * 0.5
        b = (p[2] / radius + 1) * 0.5
        colors.append(Gf.Vec3f(r, g, b))
    
    return points, face_vertex_counts, face_vertex_indices, uvs, uv_indices, colors


def compute_vertex_normals(
    points: list,
    face_vertex_counts: list,
    face_vertex_indices: list
) -> list:
    """Compute smooth vertex normals by averaging face normals."""
    normals = [Gf.Vec3f(0, 0, 0) for _ in points]
    
    idx = 0
    for count in face_vertex_counts:
        face_points = [points[face_vertex_indices[idx + i]] for i in range(count)]
        
        # Compute face normal (Newell's method for robustness)
        normal = Gf.Vec3f(0, 0, 0)
        for i in range(count):
            curr = face_points[i]
            next_pt = face_points[(i + 1) % count]
            normal += Gf.Vec3f(
                (curr[1] - next_pt[1]) * (curr[2] + next_pt[2]),
                (curr[2] - next_pt[2]) * (curr[0] + next_pt[0]),
                (curr[0] - next_pt[0]) * (curr[1] + next_pt[1])
            )
        
        # Accumulate to vertices
        for i in range(count):
            vert_idx = face_vertex_indices[idx + i]
            normals[vert_idx] += normal
        
        idx += count
    
    # Normalize
    for i, n in enumerate(normals):
        length = Gf.Dot(n, n) ** 0.5
        if length > 1e-8:
            normals[i] = n / length
    
    return normals


def create_subdivided_sphere_usd(
    stage: Usd.Stage,
    subdivision_scheme: str = "catmullClark",
    num_latitude: int = 8,
    num_longitude: int = 16
):
    """Create a USD file with a subdivision sphere and primvars."""
    
    stage.SetMetadata("upAxis", "Y")
    
    # Create the mesh
    mesh = UsdGeom.Mesh.Define(stage, "/World/Sphere")
    
    # Generate geometry
    points, fvc, fvi, uvs, uv_indices, colors = create_uv_sphere(
        num_latitude, num_longitude
    )
    normals = compute_vertex_normals(points, fvc, fvi)
    
    # Set topology
    mesh.GetPointsAttr().Set(Vt.Vec3fArray(points))
    mesh.GetFaceVertexCountsAttr().Set(Vt.IntArray(fvc))
    mesh.GetFaceVertexIndicesAttr().Set(Vt.IntArray(fvi))
    
    # Set subdivision scheme
    mesh.GetSubdivisionSchemeAttr().Set(subdivision_scheme)
    
    # Normals (vertex interpolation)
    mesh.GetNormalsAttr().Set(Vt.Vec3fArray(normals))
    mesh.SetNormalsInterpolation(UsdGeom.Tokens.vertex)
    
    # UVs (face-varying, indexed)
    uv_primvar = UsdGeom.PrimvarsAPI(mesh).CreatePrimvar(
        "st",
        Sdf.ValueTypeNames.TexCoord2fArray,
        UsdGeom.Tokens.faceVarying
    )
    uv_primvar.Set(Vt.Vec2fArray(uvs))
    uv_primvar.SetIndices(Vt.IntArray(uv_indices))
    
    # Display color (vertex interpolation, non-indexed)
    color_primvar = UsdGeom.PrimvarsAPI(mesh).CreatePrimvar(
        "displayColor",
        Sdf.ValueTypeNames.Color3fArray,
        UsdGeom.Tokens.vertex
    )
    color_primvar.Set(Vt.Vec3fArray(colors))

    # Add a uniform primvar to test that code path. We'll just make it
    # the face index for simplicity.
    uniform_primvar = UsdGeom.PrimvarsAPI(mesh).CreatePrimvar(
        "myUniform",
        Sdf.ValueTypeNames.FloatArray,
        UsdGeom.Tokens.uniform
    )
    uniform_primvar.Set(Vt.FloatArray([float(x) for x in range(len(fvc))]))
    

class TestSubdivision(unittest.TestCase):
    def test_Basic(self):
        stage = Usd.Stage.CreateInMemory()
        create_subdivided_sphere_usd(stage, num_latitude=4, num_longitude=8)

        render_settings = HydraPassthrough.RenderSettings()
        render_settings.refineLevel = 1

        m = HydraPassthrough.RenderManager()
        m.Initialize(render_settings)
        m.Render(stage)
        md = m.GetRenderData().ExtractRenderDataCopy()
        m.Cleanup()

        prefix = HydraPassthrough.RenderManager.GetSceneDelegateId()
        self.assertEqual(md.GetMeshCount(), 1)
        mesh = md.GetMesh(Sdf.Path(prefix.AppendPath('World/Sphere')))
        self.assertEqual(mesh.id, Sdf.Path(prefix.AppendPath('World/Sphere')))

        # Check that we have the expected number of points and faces after subdivision
        #
        # The sphere has 2 poles and 3 rings of 8 vertices each, for a total of 2 + 3*8 = 26 vertices.
        # This makes 32 faces (8 triangles at top, 16 quads in the middle, 8 triangles at the bottom)
        # Using Euler characteristics for a sphere (V - E + F = 2), E = 26 + 32 - 2 = 56 edges
        #
        # After one level of Catmull-Clark subdivision we expect, len(points) = V + E + F = 26 + 56 + 32 = 114 points
        #
        # The buffers from opensubdiv also contain the original points at the beginning of the buffer, and the indices
        # take this into account and start after those points. I should say coarse values, because this is also the case for
        # normals, uvs, etc. Anything subdivided has its coarse values at the start.
        #
        # In this case, the buffer has length 140, 114 subdivided points + 26 original points. The vertex indices
        # are in the range [26, 140). There are 112 quads, converted into 224 triangles.
        self.assertEqual(len(mesh.GetPoints().GetValue()), 140)
        self.assertEqual(len(mesh.GetFaceVertexIndices().GetValue()), 112 * 6) # 6 tri verts per quad
        for idx in mesh.GetFaceVertexIndices().GetValue():
            self.assertGreaterEqual(idx, 26)
            self.assertLess(idx, 140)

        # Check that we got the primitive params and edge indices
        self.assertEqual(len(mesh.GetPrimitiveParams().GetValue()), 112) # one item per face
        self.assertEqual(len(mesh.GetEdgeIndices().GetValue()), 112) # one item per triangle

        # The first value in each primitive param entry should be the original face index,
        # which should be in the range [0, 32)
        #
        # Note that each face index value in primitive params is bit-packed with other flags,
        # so we need to shift it down to get the original face index.
        # See HdMeshUtil::EncodeCoarseFaceParam in the Hydra codebase for details.
        for pp in mesh.GetPrimitiveParams().GetValue():
            self.assertGreaterEqual(pp[0] >> 2, 0)
            self.assertLess(pp[0] >> 2, 32)

        # Check that primvars are present
        primvar_names = [x.name for x in mesh.GetAllPrimvars()]
        self.assertIn('st', primvar_names)
        self.assertIn('displayColor', primvar_names)
        self.assertIn('normals', primvar_names)
        self.assertIn('myUniform', primvar_names)
        # Check that the primvars have the expected number of values
        self.assertEqual(mesh.GetPrimvar('st').data.GetArraySize(), 150) # uvs had 29 original verts
        self.assertEqual(mesh.GetPrimvar('displayColor').data.GetArraySize(), 140)
        self.assertEqual(mesh.GetPrimvar('normals').data.GetArraySize(), 140)
        # uniform primvars are not subdivided, implemenations should index by original face index
        self.assertEqual(mesh.GetPrimvar('myUniform').data.GetArraySize(), 32) # one value per original face
        # check that interpolation is preserved
        self.assertEqual(mesh.GetPrimvar('st').interpolation, UsdGeom.Tokens.faceVarying)
        self.assertEqual(mesh.GetPrimvar('displayColor').interpolation, UsdGeom.Tokens.vertex)
        self.assertEqual(mesh.GetPrimvar('normals').interpolation, UsdGeom.Tokens.vertex)
        self.assertEqual(mesh.GetPrimvar('myUniform').interpolation, UsdGeom.Tokens.uniform)

        # Check that we got an index buffer for st, it is face varying so needs its
        # own indices in the subdivided mesh.
        primvar_indices = mesh.GetFaceVaryingChannels()
        self.assertEqual(len(primvar_indices), 1)
        found = False
        for pi in primvar_indices:
            channel = pi.channel
            indices = pi.GetIndices()
            names = pi.GetPrimvarNames()
            if 'st' in names:
                found = True
                self.assertEqual(names, ['st'])
                self.assertEqual(channel, 0)
                self.assertEqual(indices.GetArraySize(), 112) # one item per face
                self.assertEqual(indices.GetElementShape(), HydraPassthrough.ValueDescriptor.ElementShape.Vec4)
                self.assertEqual(indices.GetScalarType(), HydraPassthrough.ValueDescriptor.ScalarType.Integer)
                for idx in indices.GetValue():
                    for i in [0, 1, 2, 3]: # quads only
                        self.assertGreaterEqual(idx[i], 29) # uvs had 29 original verts
                        self.assertLess(idx[i], 150)
        self.assertTrue(found, "Expected to find face-varying indices for 'st' primvar")


if __name__ == "__main__":
    unittest.main()
