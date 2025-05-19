set(PXR_PREFIX pxr/exec)
set(PXR_PACKAGE execGeom)

pxr_library(execGeom
    LIBRARIES
        gf
        tf
        execUsd
        usdGeom
        
    PUBLIC_HEADERS
        api.h

    PUBLIC_CLASSES
        tokens 

    PRIVATE_HEADERS

    PRIVATE_CLASSES

    CPPFILES
        xformable.cpp

    RESOURCE_FILES
        plugInfo.json
)

#
# Tests
#

pxr_build_test(testExecGeomXformable
    LIBRARIES
        tf
        trace
        esf
        exec
        execGeom
        execUsd
        sdf
        usd
        usdGeom
    CPPFILES
        testenv/testExecGeomXformable.cpp
)

pxr_register_test(testExecGeomXformable
    COMMAND "${CMAKE_INSTALL_PREFIX}/tests/testExecGeomXformable"
    EXPECTED_RETURN_CODE 0
    POST_COMMAND "${PYTHON_EXECUTABLE} normalizeDotFile.py testCompiler.dot testCompiler.out"
    DIFF_COMPARE testCompiler.out
    TESTENV testExecGeomXformable
)

pxr_install_test_dir(
    SRC testenv/testExecGeomXformable
    DEST testExecGeomXformable
)

pxr_build_test(testExecGeomXformable_Perf
    LIBRARIES
        tf
        trace
        esf
        exec
        execGeom
        execUsd
        sdf
        usd
        usdGeom
    CPPFILES
        testenv/testExecGeomXformable_Perf.cpp
)

pxr_register_test(testExecGeomXformable_Perf
    COMMAND "${CMAKE_INSTALL_PREFIX}/tests/testExecGeomXformable_Perf 5 7"
    EXPECTED_RETURN_CODE 0
    ENV TF_FATAL_VERIFY=1
)
