//
// Copyright 2023 Pixar
//
// Licensed under the terms set forth in the LICENSE.txt file available at
// https://openusd.org/license.
//
//

#include "pxr/imaging/hdSt/basisCurvesShaderKey.h"
#include "pxr/imaging/hdSt/codeGen.h"
#include "pxr/imaging/hdSt/drawItem.h"
#include "pxr/imaging/hdSt/fallbackLightingShader.h"
#include "pxr/imaging/hdSt/geometricShader.h"
#include "pxr/imaging/hdSt/glslfxShader.h"
#include "pxr/imaging/hdSt/materialNetworkShader.h"
#include "pxr/imaging/hdSt/meshShaderKey.h"
#include "pxr/imaging/hdSt/package.h"
#include "pxr/imaging/hdSt/pointsShaderKey.h"
#include "pxr/imaging/hdSt/renderDelegate.h"
#include "pxr/imaging/hdSt/renderPassShader.h"
#include "pxr/imaging/hdSt/resourceBinder.h"
#include "pxr/imaging/hdSt/resourceRegistry.h"

#include "pxr/imaging/hd/drawingCoord.h"
#include "pxr/imaging/hd/driver.h"
#include "pxr/imaging/hd/basisCurves.h"
#include "pxr/imaging/hd/renderIndex.h"
#include "pxr/imaging/hd/rprimSharedData.h"
#include "pxr/imaging/hd/tokens.h"

#include "pxr/imaging/hgi/tokens.h"

#include "pxr/imaging/glf/testGLContext.h"
#include "pxr/imaging/hio/glslfx.h"
#include "pxr/base/tf/errorMark.h"
#include "pxr/base/tf/staticTokens.h"

#include <iostream>

PXR_NAMESPACE_USING_DIRECTIVE

TF_DEFINE_PRIVATE_TOKENS(
    _tokens,
    (color)
    (faceVisibility)
    (normals)
    (smoothNormals)
    (points)
    (primID)
    (dispTextureCoord)
    (translate)
    (transform)
    (transformInverse)
    (widths)
    ((_float, "float"))
    (vec2)
    (vec3)
    (vec4)
    (mat4)
    ((_double, "double"))
    (dvec2)
    (dvec3)
    (dvec4)
    (dmat4)
);

static void
DumpShaderSource(const std::string& source)
{
    std::istringstream stream{source};
    for (std::string line; std::getline(stream, line); ) {
        // Remove glslfx source line comments
        if (!TfStringStartsWith(line, "// line ")) {
            std::cout << line << '\n';
        }
    }
}

static bool
CodeGenTest(HdStResourceRegistrySharedPtr const &registry,
    HdSt_ShaderKey const &key, bool instance, bool smoothNormals)
{
    TfErrorMark mark;

    // create drawItem
    HdRprimSharedData sharedData(HdDrawingCoord::DefaultNumSlots);
    sharedData.instancerLevels = 0;
    HdStDrawItem drawItem(&sharedData);


    HdDrawingCoord *drawingCoord = drawItem.GetDrawingCoord();

    HdSt_GeometricShaderSharedPtr geometricShader = 
        HdSt_GeometricShader::Create(key, registry);

    // topology
    {
        HdBufferSpecVector bufferSpecs;
        bufferSpecs.emplace_back( HdTokens->indices,
                                  HdTupleType { HdTypeInt32, 1 });

        const auto primType = geometricShader->GetPrimitiveType();
        switch(primType) {
            case HdSt_GeometricShader::PrimitiveType::PRIM_MESH_COARSE_TRIANGLES:
            {
                // bind primitiveParam and edgeIndices buffers since code gen
                // relies on these binding points to be present for meshes.
                bufferSpecs.emplace_back( HdTokens->primitiveParam,
                                          HdTupleType { HdTypeInt32, 1 });

                bufferSpecs.emplace_back(HdTokens->edgeIndices,
                                         HdTupleType{HdTypeInt32, 1});
                break;
            }
            case HdSt_GeometricShader::PrimitiveType::PRIM_MESH_COARSE_QUADS:
            {
                // bind primitiveParam and edgeIndices buffers since code gen
                // relies on these binding points to be present for meshes.
                bufferSpecs.emplace_back( HdTokens->primitiveParam,
                                          HdTupleType { HdTypeInt32, 1 });

                bufferSpecs.emplace_back(HdTokens->edgeIndices,
                                         HdTupleType{HdTypeInt32Vec2, 1});
                break;
            }
            default:
            {
                // do nothing
            }
        }
        HdBufferArrayRangeSharedPtr range =
                registry->AllocateNonUniformBufferArrayRange(
                    HdTokens->topology, bufferSpecs,
                    HdBufferArrayUsageHintBitsIndex);

        sharedData.barContainer.Set(
            drawingCoord->GetTopologyIndex(), range);
    }

    // constant primvars
    {
        HdBufferSpecVector bufferSpecs;
        bufferSpecs.emplace_back( _tokens->transform,
                                  HdTupleType { HdTypeFloatMat4, 1 });
        bufferSpecs.emplace_back( _tokens->transformInverse,
                                  HdTupleType { HdTypeFloatMat4, 1 });
        bufferSpecs.emplace_back( _tokens->color,
                                  HdTupleType { HdTypeFloatVec4, 1 });
        bufferSpecs.emplace_back( _tokens->primID,
                                  HdTupleType { HdTypeFloatVec4, 1 });
        bufferSpecs.emplace_back( _tokens->widths,
                                  HdTupleType { HdTypeFloat, 1 });
        HdBufferArrayRangeSharedPtr range =
            registry->AllocateShaderStorageBufferArrayRange(
                HdTokens->primvar, bufferSpecs,
                HdBufferArrayUsageHintBitsStorage);

        sharedData.barContainer.Set(
            drawingCoord->GetConstantPrimvarIndex(), range);
    }

    // element primvars
    {
        HdBufferSpecVector bufferSpecs;
        bufferSpecs.emplace_back( _tokens->faceVisibility,
                                  HdTupleType { HdTypeFloat, 1 });
        HdBufferArrayRangeSharedPtr range =
            registry->AllocateNonUniformBufferArrayRange(
                HdTokens->primvar, bufferSpecs,
                HdBufferArrayUsageHintBitsStorage);

        sharedData.barContainer.Set(
            drawingCoord->GetElementPrimvarIndex(), range);
    }

    // vertex primvars
    {
        HdBufferSpecVector bufferSpecs;
        bufferSpecs.emplace_back( _tokens->points,
                                  HdTupleType { HdTypeFloatVec3, 1 });
        // XXX: The order of emitting multiple attribute is arbitrary
        // since HdBufferResourceMap uses hash_map of TfToken.
        // The resulting code becomes unstable if we have more than 1
        // primvars in the same category. We need to fix it.
        if (smoothNormals) {
            bufferSpecs.emplace_back( _tokens->smoothNormals,
                                      HdTupleType { HdTypeFloatVec3, 1 });
        } else {
            bufferSpecs.emplace_back( _tokens->normals,
                                      HdTupleType { HdTypeFloatVec3, 1 });
        }

        HdBufferArrayRangeSharedPtr range =
            registry->AllocateNonUniformBufferArrayRange(
                HdTokens->primvar, bufferSpecs,
                HdBufferArrayUsageHintBitsVertex);

        sharedData.barContainer.Set(
            drawingCoord->GetVertexPrimvarIndex(), range);
    }

    // facevarying primvars are allowed only for mesh prim types
    const auto primType = geometricShader->GetPrimitiveType();
    if (HdSt_GeometricShader::IsPrimTypeMesh(primType)) {
        HdBufferSpecVector bufferSpecs;
        bufferSpecs.emplace_back( _tokens->dispTextureCoord,
                                  HdTupleType { HdTypeFloatVec2, 1 });

        HdBufferArrayRangeSharedPtr range =
            registry->AllocateNonUniformBufferArrayRange(
                HdTokens->primvar, bufferSpecs,
                HdBufferArrayUsageHintBitsStorage);

        sharedData.barContainer.Set(
            drawingCoord->GetFaceVaryingPrimvarIndex(), range);
    }

    if (instance) {
        // instance primvars
        {
            HdBufferSpecVector bufferSpecs;
            bufferSpecs.emplace_back(
                HdInstancerTokens->instanceTranslations,
                HdTupleType { HdTypeFloatVec3, 1 });
            HdBufferArrayRangeSharedPtr range =
                registry->AllocateNonUniformBufferArrayRange(
                    HdTokens->primvar, bufferSpecs,
                    HdBufferArrayUsageHintBitsStorage);

            drawingCoord->SetInstancePrimvarBaseIndex(/*hard-coded*/8);
            sharedData.instancerLevels = 1;

            sharedData.barContainer.Set(
                drawingCoord->GetInstancePrimvarIndex(0), range);
        }
        // instance index
        {
            HdBufferSpecVector bufferSpecs;
            bufferSpecs.emplace_back( HdInstancerTokens->instanceIndices,
                                      HdTupleType { HdTypeInt32, 1 });
            bufferSpecs.emplace_back( HdInstancerTokens->culledInstanceIndices,
                                      HdTupleType { HdTypeInt32, 1 });
            HdBufferArrayRangeSharedPtr range =
                registry->AllocateNonUniformBufferArrayRange(
                    HdTokens->primvar, bufferSpecs,
                    HdBufferArrayUsageHintBitsIndex);
            sharedData.barContainer.Set(
                drawingCoord->GetInstanceIndexIndex(), range);
        }
    }

    HdSt_ResourceBinder binder;
    HdStBindingRequestVector empty;

    HdSt_MaterialNetworkShaderSharedPtr _fallbackMaterialNetworkShader;
    HioGlslfxSharedPtr glslfx = std::make_shared<HioGlslfx>(
                HdStPackageFallbackMaterialNetworkShader());
    _fallbackMaterialNetworkShader = std::make_shared<HdStGLSLFXShader>(glslfx);

    HdStShaderCodeSharedPtrVector shaders(3);
    shaders[0].reset(new HdStRenderPassShader());
    shaders[1].reset(new HdSt_FallbackLightingShader());
    shaders[2] = _fallbackMaterialNetworkShader;

    HdSt_ResourceBinder::MetaData::DrawingCoordBufferBinding dcBinding;

    std::unique_ptr<HdSt_ResourceBinder::MetaData> metaData =
        std::make_unique<HdSt_ResourceBinder::MetaData>();
    
    binder.ResolveBindings(&drawItem,
                           shaders,
                           metaData.get(),
                           dcBinding,
                           /*instanced=*/true, empty,
                           registry->GetHgi()->GetCapabilities());

    HdSt_CodeGen codeGen(geometricShader, shaders,
                         drawItem.GetMaterialTag(), std::move(metaData));

    codeGen.Compile(registry.get());

    std::cout <<
        "-------------------------------------------------------\n"
              << key.GetGlslfxString()
              <<
        "-------------------------------------------------------\n";
    std::cout <<
        "=======================================================\n"
        "  VERTEX SHADER                                        \n"
        "=======================================================\n";
    DumpShaderSource(codeGen.GetVertexShaderSource());
    std::cout <<
        "=======================================================\n"
        "  TESS CONTROL SHADER                                  \n"
        "=======================================================\n";
    DumpShaderSource(codeGen.GetTessControlShaderSource());
    std::cout <<
        "=======================================================\n"
        "  TESS EVAL SHADER                                     \n"
        "=======================================================\n";
    DumpShaderSource(codeGen.GetTessEvalShaderSource());
    std::cout <<
        "=======================================================\n"
        "  GEOMETRY SHADER                                      \n"
        "=======================================================\n";
    DumpShaderSource(codeGen.GetGeometryShaderSource());
    std::cout <<
        "=======================================================\n"
        "  FRAGMENT SHADER                                      \n"
        "=======================================================\n";
    DumpShaderSource(codeGen.GetFragmentShaderSource());

    return TF_VERIFY(mark.IsClean());
}

bool
TestShader(HdStResourceRegistrySharedPtr const &registry,
    HdSt_ShaderKey const &key, bool instance, bool smoothNormals)
{
    bool success = true;
    success &= CodeGenTest(registry, key, instance, smoothNormals);
    return success;
}

int main(int argc, char *argv[])
{
    GlfTestGLContext::RegisterGLContextCallbacks();
    GlfSharedGLContextScopeHolder sharedContext;

    bool success = true;

    bool smoothNormals = false;
    bool doubleSided = false;
    bool faceVarying = false;
    bool topologicalVisibility = false;
    bool blendWireframeColor = false;
    bool instance = false;
    bool mesh = false;
    bool curves = false;
    bool points = false;
    HdMeshGeomStyle geomStyle = HdMeshGeomStyleSurf;

    for (int i=0; i<argc; ++i) {
        std::string arg(argv[i]);

        if (arg == "--smoothNormals") {
            smoothNormals = true;
        } else if (arg == "--doubleSided") {
            doubleSided = true;
        } else if (arg == "--faceVarying") {
            faceVarying = true;
        } else if (arg == "--blendWireframe") {
            blendWireframeColor = true;
        } else if (arg == "--instance") {
            instance = true;
        } else if (arg == "--mesh") {
            mesh = true;
        } else if (arg == "--curves") {
            curves = true;
        } else if (arg == "--points") {
            points = true;
        } else if (arg == "--edgeOnly") {
            geomStyle = HdMeshGeomStyleEdgeOnly;
        }
    }

    HgiUniquePtr const hgi = Hgi::CreatePlatformDefaultHgi();
    HdDriver driver{HgiTokens->renderDriver, VtValue(hgi.get())};
    HdStRenderDelegate renderDelegate;
    std::unique_ptr<HdRenderIndex> const index(
        HdRenderIndex::New(&renderDelegate, {&driver}));
    HdStResourceRegistrySharedPtr const registry =
        std::static_pointer_cast<HdStResourceRegistry>(
            index->GetResourceRegistry());

    // mesh
    if (mesh) {
        success &= TestShader(
            registry,
            HdSt_MeshShaderKey(
                HdSt_GeometricShader::PrimitiveType::PRIM_MESH_COARSE_TRIANGLES, 
                /* shadingTerminal */ TfToken(), 
                smoothNormals ? HdSt_MeshShaderKey::NormalSourceSmooth :
                    HdSt_MeshShaderKey::NormalSourceFlat,
                HdInterpolationVertex,
                HdCullStyleNothing,
                geomStyle,
                HdSt_GeometricShader::FvarPatchType::PATCH_COARSE_TRIANGLES,
                0,
                doubleSided,
                /* hasBuiltinBarycentics */ false,
                /* hasMetalTessellation */ false,
                /* hasCustomDisplacement */ false,
                faceVarying,
                topologicalVisibility,
                blendWireframeColor,
                /* hasMirroredTransform */ false,
                instance,
                /* enableScalarOverride */ true,
                /* pointsShadingEnabled */ false,
                /* forceOpaqueEdges */ true,
                /* surfaceEdgeIds */ true,
                /* nativeRoundPoints */ true),
                 instance, smoothNormals);
        success &= TestShader(
            registry,
            HdSt_MeshShaderKey(
                HdSt_GeometricShader::PrimitiveType::PRIM_MESH_COARSE_QUADS, 
                /* shadingTerminal */ TfToken(), 
                smoothNormals ? HdSt_MeshShaderKey::NormalSourceSmooth :
                    HdSt_MeshShaderKey::NormalSourceFlat,
                HdInterpolationVertex,
                HdCullStyleNothing,
                geomStyle,
                HdSt_GeometricShader::FvarPatchType::PATCH_COARSE_QUADS,
                0,
                doubleSided,
                /* hasBuiltinBarycentics */ false,
                /* hasMetalTessellation */ false,
                /* hasCustomDisplacement */ false, 
                faceVarying, topologicalVisibility,
                blendWireframeColor,
                /* hasMirroredTransform */ false,
                instance,
                /* enableScalarOverride */ true,
                /* pointsShadingEnabled */ false,
                /* forceOpaqueEdges */ true,
                /* surfaceEdgeIds */ true,
                /* nativeRoundPoints */ true),
                 instance, smoothNormals);
    }

    // curves
    if (curves) {
        success &= TestShader(
            registry,
            HdSt_BasisCurvesShaderKey(HdTokens->cubic,
                            HdTokens->bezier,
                            HdSt_BasisCurvesShaderKey::WIRE,
                            HdSt_BasisCurvesShaderKey::HAIR, false, 
                            true,
                            HdBasisCurvesReprDescTokens->surfaceShader,
                            topologicalVisibility,
                            /* pointsShadingEnabled */ false,
                            /* hasMetalTessellation */ false,
                            /* nativeRoundPoints */ true),
                             instance, false);
    }

    // points
    if (points) {
        success &= TestShader(registry, HdSt_PointsShaderKey(
            /* nativeRoundPoints */ false), instance, false);
    }

    if (success) {
        std::cout << "OK" << std::endl;
        return EXIT_SUCCESS;
    } else {
        std::cout << "FAILED" << std::endl;
        return EXIT_FAILURE;
    }
}
