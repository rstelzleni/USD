set(PXR_PREFIX pxr/usd)
set(PXR_PACKAGE usdGeom)

pxr_library(usdGeom
    INCLUDE_SCHEMA_FILES

    LIBRARIES
        js
        tf
        plug
        vt
        sdf
        trace
        usd
        work
        TBB::tbb

    PUBLIC_CLASSES
        debugCodes
        bboxCache
        constraintTarget
        xformCache
        boundableComputeExtent
        metrics
        primvar
        xformOp

    PUBLIC_HEADERS
        api.h


    PRIVATE_CLASSES
        samplingUtils

    PYMODULE_CPPFILES
        wrapBBoxCache.cpp
        wrapConstraintTarget.cpp
        wrapMetrics.cpp
        wrapPrimvar.cpp
        wrapXformCache.cpp
        wrapXformOp.cpp

    PYMODULE_FILES
        __init__.py

    DOXYGEN_FILES
        overview.dox
        snippets.dox
        images/USDCurveBasisMatrix.png
        images/USDCurveHydra.png
        images/USDCurvePrimvars.png
        images/USDTetMeshRightHanded.svg
        images/USDTetMeshLeftHanded.svg
        examples_usdGeom.cpp
)

pxr_test_scripts(
    testenv/testUsdGeomBasisCurves.py
    testenv/testUsdGeomBBoxCache.py
    testenv/testUsdGeomCamera.py
    testenv/testUsdGeomComputeAtTime.py
    testenv/testUsdGeomConstraintTarget.py
    testenv/testUsdGeomConsts.py
    testenv/testUsdGeomCurves.py
    testenv/testUsdGeomExtentFromPlugins.py
    testenv/testUsdGeomExtentTransform.py
    testenv/testUsdGeomHermiteCurves.py
    testenv/testUsdGeomImageable.py
    testenv/testUsdGeomMesh.py
    testenv/testUsdGeomMetrics.py
    testenv/testUsdGeomMotionAPI.py
    testenv/testUsdGeomNoPlugLoad.py
    testenv/testUsdGeomPointInstancer.py
    testenv/testUsdGeomPrimvar.py
    testenv/testUsdGeomPurposeVisibility.py
    testenv/testUsdGeomSchemata.py
    testenv/testUsdGeomSubset.py
    testenv/testUsdGeomTetMesh.py
    testenv/testUsdGeomXformable.py
    testenv/testUsdGeomXformCommonAPI.py
)

pxr_build_test(testUsdGeomXformCache
    LIBRARIES
        usd
        usdGeom
    CPPFILES
        testenv/testUsdGeomXformCache.cpp
)


pxr_build_test(testUsdGeomCreateAttribute
    LIBRARIES
        usd
        usdGeom
    CPPFILES
        testenv/testUsdGeomCreateAttribute.cpp
)

pxr_build_test(testUsdGeomIsA
    LIBRARIES
        usd
        usdGeom
    CPPFILES
        testenv/testUsdGeomIsA.cpp
)

pxr_build_test(testUsdGeomHasAPI
    LIBRARIES
        usd
        usdGeom
    CPPFILES
        testenv/testUsdGeomHasAPI.cpp
)

pxr_install_test_dir(
    SRC testenv/testUsdGeomBasisCurves
    DEST testUsdGeomBasisCurves
)

pxr_install_test_dir(
    SRC testenv/testUsdGeomBBoxCache
    DEST testUsdGeomBBoxCache
)

pxr_install_test_dir(
    SRC testenv/testUsdGeomCamera
    DEST testUsdGeomCamera
)

pxr_install_test_dir(
    SRC testenv/testUsdGeomConsts
    DEST testUsdGeomConsts
)

pxr_install_test_dir(
    SRC testenv/testUsdGeomPointInstancer
    DEST testUsdGeomPointInstancer
)

pxr_install_test_dir(
    SRC testenv/testUsdGeomSubset
    DEST testUsdGeomSubset)

pxr_install_test_dir(
    SRC testenv/testUsdGeomBBoxCache
    DEST testUsdGeomBBoxCache
)

pxr_install_test_dir(
    SRC testenv/testUsdGeomExtentFromPlugins
    DEST testUsdGeomExtentFromPlugins
)

pxr_install_test_dir(
    SRC testenv/testUsdGeomExtentTransform
    DEST testUsdGeomExtentTransform
)

pxr_install_test_dir(
    SRC testenv/testUsdGeomComputeAtTime
    DEST testUsdGeomComputeAtTime
)

pxr_install_test_dir(
    SRC testenv/testUsdGeomImageable
    DEST testUsdGeomImageable
)

pxr_install_test_dir(
    SRC testenv/testUsdGeomMesh
    DEST testUsdGeomMesh
)

pxr_install_test_dir(
    SRC testenv/testUsdGeomTetMesh
    DEST testUsdGeomTetMesh
)

pxr_register_test(testUsdGeomBasisCurves
    PYTHON
    COMMAND "${CMAKE_INSTALL_PREFIX}/tests/testUsdGeomBasisCurves"
    EXPECTED_RETURN_CODE 0
)

pxr_register_test(testUsdGeomBBoxCache
    PYTHON
    COMMAND "${CMAKE_INSTALL_PREFIX}/tests/testUsdGeomBBoxCache"
    EXPECTED_RETURN_CODE 0
    ENV 
        TF_DEBUG=USDGEOM_BBOX
        USD_ASSIGN_PROTOTYPES_DETERMINISTICALLY=1
)

pxr_register_test(testUsdGeomCamera
    PYTHON
    COMMAND "${CMAKE_INSTALL_PREFIX}/tests/testUsdGeomCamera"
    EXPECTED_RETURN_CODE 0
)

pxr_register_test(testUsdGeomConstraintTarget
    PYTHON
    COMMAND "${CMAKE_INSTALL_PREFIX}/tests/testUsdGeomConstraintTarget"
    EXPECTED_RETURN_CODE 0
)

pxr_register_test(testUsdGeomConsts
    PYTHON
    COMMAND "${CMAKE_INSTALL_PREFIX}/tests/testUsdGeomConsts"
    EXPECTED_RETURN_CODE 0
)

pxr_register_test(testUsdGeomCreateAttribute
    COMMAND "${CMAKE_INSTALL_PREFIX}/tests/testUsdGeomCreateAttribute"
    EXPECTED_RETURN_CODE 0
)

pxr_register_test(testUsdGeomCurves
    PYTHON
    COMMAND "${CMAKE_INSTALL_PREFIX}/tests/testUsdGeomCurves"
    EXPECTED_RETURN_CODE 0
)

pxr_register_test(testUsdGeomMesh
    PYTHON
    COMMAND "${CMAKE_INSTALL_PREFIX}/tests/testUsdGeomMesh"
    EXPECTED_RETURN_CODE 0
)

pxr_register_test(testUsdGeomTetMesh
    PYTHON
    COMMAND "${CMAKE_INSTALL_PREFIX}/tests/testUsdGeomTetMesh"
    DIFF_COMPARE tetMeshRH.usda
    DIFF_COMPARE tetMeshLH.usda
)

pxr_register_test(testUsdGeomMetrics
    PYTHON
    COMMAND "${CMAKE_INSTALL_PREFIX}/tests/testUsdGeomMetrics"
    EXPECTED_RETURN_CODE 0
)

pxr_register_test(testUsdGeomSubset
    PYTHON
    COMMAND "${CMAKE_INSTALL_PREFIX}/tests/testUsdGeomSubset"
    EXPECTED_RETURN_CODE 0
)

pxr_register_test(testUsdGeomIsA
    COMMAND "${CMAKE_INSTALL_PREFIX}/tests/testUsdGeomIsA"
    EXPECTED_RETURN_CODE 0
)

pxr_register_test(testUsdGeomHasAPI
    COMMAND "${CMAKE_INSTALL_PREFIX}/tests/testUsdGeomHasAPI"
    EXPECTED_RETURN_CODE 0
)

# This tests functionality when usdGeom has not been loaded.  That isn't
# possible with a monolithic library.
if(NOT PXR_BUILD_MONOLITHIC)
pxr_register_test(testUsdGeomNoPlugLoad
    PYTHON
    COMMAND "${CMAKE_INSTALL_PREFIX}/tests/testUsdGeomNoPlugLoad"
    EXPECTED_RETURN_CODE 0
)
endif()

pxr_register_test(testUsdGeomPrimvar
    PYTHON
    COMMAND "${CMAKE_INSTALL_PREFIX}/tests/testUsdGeomPrimvar"
    EXPECTED_RETURN_CODE 0
)

pxr_register_test(testUsdGeomPointInstancer
    PYTHON
    COMMAND "${CMAKE_INSTALL_PREFIX}/tests/testUsdGeomPointInstancer"
    EXPECTED_RETURN_CODE 0
)

pxr_register_test(testUsdGeomPurposeVisibility
    PYTHON
    COMMAND "${CMAKE_INSTALL_PREFIX}/tests/testUsdGeomPurposeVisibility"
    EXPECTED_RETURN_CODE 0
)

pxr_register_test(testUsdGeomSchemata
    PYTHON
    COMMAND "${CMAKE_INSTALL_PREFIX}/tests/testUsdGeomSchemata"
    EXPECTED_RETURN_CODE 0
)

pxr_register_test(testUsdGeomXformable
    PYTHON
    COMMAND "${CMAKE_INSTALL_PREFIX}/tests/testUsdGeomXformable"
    EXPECTED_RETURN_CODE 0
)

pxr_register_test(testUsdGeomXformCache
    COMMAND "${CMAKE_INSTALL_PREFIX}/tests/testUsdGeomXformCache"
    EXPECTED_RETURN_CODE 0 
)

pxr_register_test(testUsdGeomXformCommonAPI
    PYTHON
    COMMAND "${CMAKE_INSTALL_PREFIX}/tests/testUsdGeomXformCommonAPI"
    EXPECTED_RETURN_CODE 0
)

pxr_register_test(testUsdGeomExtentFromPlugins
    PYTHON
    COMMAND "${CMAKE_INSTALL_PREFIX}/tests/testUsdGeomExtentFromPlugins"
    EXPECTED_RETURN_CODE 0
)

pxr_register_test(testUsdGeomExtentTransform
    PYTHON
    COMMAND "${CMAKE_INSTALL_PREFIX}/tests/testUsdGeomExtentTransform"
    EXPECTED_RETURN_CODE 0
)

pxr_register_test(testUsdGeomComputeAtTime
    PYTHON
    COMMAND "${CMAKE_INSTALL_PREFIX}/tests/testUsdGeomComputeAtTime"
    EXPECTED_RETURN_CODE 0
)

pxr_register_test(testUsdGeomImageable
    PYTHON
    COMMAND "${CMAKE_INSTALL_PREFIX}/tests/testUsdGeomImageable"
    EXPECTED_RETURN_CODE 0
)

