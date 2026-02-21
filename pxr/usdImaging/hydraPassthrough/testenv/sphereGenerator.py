#!/usr/bin/env python3
"""
Generate a subdivided sphere with UVs, display color, and normals.
Outputs a USD file for testing primvar handling through subdivision.
"""

import math
from pxr import Usd, UsdGeom, Vt, Gf

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
    #output_path: str = "subdivided_sphere.usda",
    subdivision_scheme: str = "catmullClark",
    num_latitude: int = 8,
    num_longitude: int = 16
):
    """Create a USD file with a subdivision sphere and primvars."""
    
    #stage = Usd.Stage.CreateNew(output_path)
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
        Vt.Vec2fArray,
        UsdGeom.Tokens.faceVarying
    )
    uv_primvar.Set(Vt.Vec2fArray(uvs))
    uv_primvar.SetIndices(Vt.IntArray(uv_indices))
    
    # Display color (vertex interpolation, non-indexed)
    color_primvar = UsdGeom.PrimvarsAPI(mesh).CreatePrimvar(
        "displayColor",
        Vt.Vec3fArray,
        UsdGeom.Tokens.vertex
    )
    color_primvar.Set(Vt.Vec3fArray(colors))
    
    #stage.GetRootLayer().Save()
    #print(f"Created {output_path}")
    #print(f"  Points: {len(points)}")
    #print(f"  Faces: {len(fvc)}")
    #print(f"  UVs: {len(uvs)} (indexed, face-varying)")
    #print(f"  Colors: {len(colors)} (vertex)")
    #print(f"  Normals: {len(normals)} (vertex)")


if __name__ == "__main__":
    import argparse
    
    parser = argparse.ArgumentParser(description="Generate a subdivided sphere USD file")
    parser.add_argument("-o", "--output", default="subdivided_sphere.usda",
                        help="Output USD file path")
    parser.add_argument("--scheme", default="catmullClark",
                        choices=["catmullClark", "loop", "bilinear", "none"],
                        help="Subdivision scheme")
    parser.add_argument("--lat", type=int, default=8,
                        help="Number of latitude divisions")
    parser.add_argument("--lon", type=int, default=16,
                        help="Number of longitude divisions")
    
    args = parser.parse_args()

    stage = Usd.Stage.CreateNew(args.output)
    
    create_subdivided_sphere_usd(
        stage,
        subdivision_scheme=args.scheme,
        num_latitude=args.lat,
        num_longitude=args.lon
    )

    stage.GetRootLayer().Save()
