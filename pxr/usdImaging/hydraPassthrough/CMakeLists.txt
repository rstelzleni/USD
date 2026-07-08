set(PXR_PREFIX pxr/usdImaging)
set(PXR_PACKAGE hydraPassthrough)

# Note that usdImaging adds a dependency on libX11 and libGL. The
# dependency is transitive, one offending lib is glf, I'm not sure
# what pulls that in, but usdImaging is the root of the tree.
#
# Ultimately, these dependencies add hundreds of MB to the final package.
#
# It's only used for scene indices, we should be able to reimplement
# those into this lib, but that also requires a lot of prim adapters
# that usdImaging also provides. This amounts to a large amount of code
# duplication. It still might be worth it, but for now I'm using
# usdImaging just to get moving.
#
pxr_library(hydraPassthrough
    LIBRARIES
        gf
        tf
        hd
        hdsi
        hdx
        sdf
        usd
        usdImaging

    PUBLIC_CLASSES
        computation
        computationInputs
        computationUtil
        constantVtBufferSource
        extCompPrimvarBufferSource
        fvarTopologyTracker
        hdTypeUtil
        instancer
        material
        materialParam
        mesh
        meshPackagingUtil
        meshTopology
        meshUtil
        normalComputations
        primUtil
        quadrangulate
        renderData
        renderDelegate
        renderManager
        rendererPlugin
        renderPass
        resourceRegistry
        subdivision
        textureDescriptor
        triangulate
        valueDescriptor
        vertexAdjacency

    PUBLIC_HEADERS
        api.h
        renderParam.h

    PYMODULE_CPPFILES
        module.cpp
        wrapMaterialParam.cpp
        wrapRenderData.cpp
        wrapRenderManager.cpp
        wrapTextureDescriptor.cpp
        wrapValueDescriptor.cpp

    PYMODULE_FILES
        __init__.py

    RESOURCE_FILES
        plugInfo.json
)

pxr_build_test(testHydraPassthroughValueDescriptor
    LIBRARIES
        tf
        gf
        vt
        hydraPassthrough
    CPPFILES
        testenv/testHydraPassthroughValueDescriptor.cpp
)

pxr_test_scripts(
    testenv/testHydraPassthroughCameraData.py
    testenv/testHydraPassthroughInstancing.py
    testenv/testHydraPassthroughMeshData.py
    testenv/testHydraPassthroughMaterialData.py
    testenv/testHydraPassthroughSubdivision.py
)

pxr_register_test(testHydraPassthroughCameraData
    PYTHON
    COMMAND "${CMAKE_INSTALL_PREFIX}/tests/testHydraPassthroughCameraData"
    EXPECTED_RETURN_CODE 0
)

pxr_register_test(testHydraPassthroughMeshData
    PYTHON
    COMMAND "${CMAKE_INSTALL_PREFIX}/tests/testHydraPassthroughMeshData"
    EXPECTED_RETURN_CODE 0
)

pxr_register_test(testHydraPassthroughMaterialData
    PYTHON
    COMMAND "${CMAKE_INSTALL_PREFIX}/tests/testHydraPassthroughMaterialData"
    EXPECTED_RETURN_CODE 0
)

pxr_register_test(testHydraPassthroughValueDescriptor
    COMMAND "${CMAKE_INSTALL_PREFIX}/tests/testHydraPassthroughValueDescriptor"
)

pxr_register_test(testHydraPassthroughSubdivision
    PYTHON
    COMMAND "${CMAKE_INSTALL_PREFIX}/tests/testHydraPassthroughSubdivision"
    EXPECTED_RETURN_CODE 0
)

pxr_register_test(testHydraPassthroughInstancing
    PYTHON
    COMMAND "${CMAKE_INSTALL_PREFIX}/tests/testHydraPassthroughInstancing"
    EXPECTED_RETURN_CODE 0
)
