set(PXR_PREFIX pxr/usd)
set(PXR_PACKAGE usdSkel)

pxr_library(usdSkel
    INCLUDE_SCHEMA_FILES

    LIBRARIES
        arch
        gf
        tf
        trace
        vt
        work
        sdf
        usd
        usdGeom
        TBB::tbb

    PUBLIC_CLASSES
        animMapper
        animQuery
        bakeSkinning
        blendShapeQuery
        cache
        debugCodes
        inbetweenShape
        skeletonQuery
        skinningQuery
        topology
        utils

    PUBLIC_HEADERS
        api.h
        binding.h

    PRIVATE_CLASSES
        animQueryImpl
        cacheImpl
        skelDefinition

    PYMODULE_FILES
        __init__.py

    PYMODULE_CPPFILES
        wrapAnimMapper.cpp
        wrapAnimQuery.cpp
        wrapBakeSkinning.cpp
        wrapBlendShapeQuery.cpp
        wrapBinding.cpp
        wrapCache.cpp
        wrapInbetweenShape.cpp
        wrapSkeletonQuery.cpp
        wrapSkinningQuery.cpp
        wrapTopology.cpp
        wrapUtils.cpp

    DOXYGEN_FILES
        overview.dox
        doxygen/apiIntro.dox
        doxygen/bestPractices.dox
        doxygen/instancing.dox
        doxygen/intro.dox
        doxygen/objectModel.dox
        doxygen/schemaOverview.dox
        doxygen/schemas.dox
        doxygen/skinnedArm.usda
        images/blendshapes.svg
        images/blendshapesWithInbetweens.svg
        images/influencesPrimvarLayout.svg
        images/unboundedInterpolationExample.svg
)

pxr_test_scripts(
    testenv/testUsdSkelAnimMapper.py
    testenv/testUsdSkelAnimQuery.py 
    testenv/testUsdSkelBakeSkinning.py
    testenv/testUsdSkelBindingAPI.py
    testenv/testUsdSkelBlendShape.py
    testenv/testUsdSkelCache.py
    testenv/testUsdSkelRoot.py
    testenv/testUsdSkelSkeletonQuery.py
    testenv/testUsdSkelSkinningQuery.py
    testenv/testUsdSkelTopology.py
    testenv/testUsdSkelUtils.py
)

pxr_install_test_dir(
    SRC testenv/testUsdSkelBakeSkinning
    DEST testUsdSkelBakeSkinning
)

pxr_install_test_dir(
    SRC testenv/testUsdSkelCache
    DEST testUsdSkelCache
)

pxr_install_test_dir(
    SRC testenv/testUsdSkelRoot
    DEST testUsdSkelRoot
)

pxr_install_test_dir(
    SRC testenv/testUsdSkelSkinningQuery
    DEST testUsdSkelSkinningQuery
)

pxr_register_test(testUsdSkelAnimMapper
    PYTHON
    COMMAND "${CMAKE_INSTALL_PREFIX}/tests/testUsdSkelAnimMapper"
    EXPECTED_RETURN_CODE 0
)

pxr_register_test(testUsdSkelAnimQuery
    PYTHON
    COMMAND "${CMAKE_INSTALL_PREFIX}/tests/testUsdSkelAnimQuery"
    EXPECTED_RETURN_CODE 0
)

pxr_register_test(testUsdSkelBakeSkinning
    PYTHON
    COMMAND "${CMAKE_INSTALL_PREFIX}/tests/testUsdSkelBakeSkinning"
    EXPECTED_RETURN_CODE 0
    DIFF_COMPARE blendshapes.baked.usda blendshapesWithNormals.baked.usda lbs.baked.usda lbs.bakedInterval.usda
)

pxr_register_test(testUsdSkelBindingAPI
    PYTHON
    COMMAND "${CMAKE_INSTALL_PREFIX}/tests/testUsdSkelBindingAPI"
    EXPECTED_RETURN_CODE 0
)

pxr_register_test(testUsdSkelCache
    PYTHON
    COMMAND "${CMAKE_INSTALL_PREFIX}/tests/testUsdSkelCache"
    EXPECTED_RETURN_CODE 0
)

pxr_register_test(testUsdSkelRoot
    PYTHON
    COMMAND "${CMAKE_INSTALL_PREFIX}/tests/testUsdSkelRoot"
    EXPECTED_RETURN_CODE 0
    DIFF_COMPARE root.computedExtents.usda
)

pxr_register_test(testUsdSkelSkeletonQuery
    PYTHON
    COMMAND "${CMAKE_INSTALL_PREFIX}/tests/testUsdSkelSkeletonQuery"
    EXPECTED_RETURN_CODE 0
)

pxr_register_test(testUsdSkelSkinningQuery
    PYTHON
    COMMAND "${CMAKE_INSTALL_PREFIX}/tests/testUsdSkelSkinningQuery"
    EXPECTED_RETURN_CODE 0
)

pxr_register_test(testUsdSkelTopology
    PYTHON
    COMMAND "${CMAKE_INSTALL_PREFIX}/tests/testUsdSkelTopology"
    EXPECTED_RETURN_CODE 0
)

pxr_register_test(testUsdSkelUtils
    PYTHON
    COMMAND "${CMAKE_INSTALL_PREFIX}/tests/testUsdSkelUtils"
    EXPECTED_RETURN_CODE 0
)

