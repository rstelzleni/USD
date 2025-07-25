set(PXR_PREFIX pxr/usd)
set(PXR_PACKAGE usd)

pxr_library(usd
    INCLUDE_SCHEMA_FILES

    LIBRARIES
        arch
        kind
        pcp
        sdf
        ar
        plug
        tf
        trace
        ts
        vt
        work
        TBB::tbb

    PUBLIC_CLASSES
        attribute
        attributeQuery
        collectionMembershipQuery
        collectionPredicateLibrary
        common
        debugCodes
        editContext
        editTarget
        errors
        flattenUtils
        inherits
        interpolation
        namespaceEditor
        notice
        object
        payloads
        prim
        primCompositionQuery
        primData
        primDataHandle
        primDefinition
        primFlags
        primRange
        primTypeInfo
        property
        references
        relationship
        resolveInfo
        resolveTarget
        resolver
        schemaBase
        schemaRegistry
        specializes
        stage
        stageCache
        stageCacheContext
        stageLoadRules
        stagePopulationMask
        timeCode
        usdFileFormat
        usdaFileFormat
        usdcFileFormat
        usdzFileFormat
        variantSets

    PYTHON_PUBLIC_CLASSES
        pyEditContext
        pyConversions

    PRIVATE_CLASSES
        clip
        clipCache
        clipSet
        clipSetDefinition
        instanceCache
        instanceKey
        interpolators
        primTypeInfoCache
        valueUtils

    PUBLIC_HEADERS
        api.h
        crateInfo.h
        zipFile.h

    PRIVATE_HEADERS
        listEditImpl.h
        wrapUtils.h
        testenv/TestUsdResolverChangedResolver.h

    PYMODULE_CPPFILES
        wrapAttribute.cpp
        wrapAttributeQuery.cpp
        wrapCollectionMembershipQuery.cpp
        wrapCommon.cpp
        wrapCrateInfo.cpp
        wrapEditContext.cpp
        wrapEditTarget.cpp
        wrapFlattenUtils.cpp
        wrapInherits.cpp
        wrapInterpolation.cpp
        wrapNamespaceEditor.cpp
        wrapNotice.cpp
        wrapObject.cpp
        wrapPayloads.cpp
        wrapPrim.cpp
        wrapPrimCompositionQuery.cpp
        wrapPrimDefinition.cpp
        wrapPrimFlags.cpp
        wrapPrimRange.cpp
        wrapPrimTypeInfo.cpp
        wrapProperty.cpp
        wrapReferences.cpp
        wrapRelationship.cpp
        wrapResolveInfo.cpp
        wrapResolveTarget.cpp
        wrapSchemaBase.cpp
        wrapSchemaRegistry.cpp
        wrapSpecializes.cpp
        wrapStage.cpp
        wrapStageCache.cpp
        wrapStageCacheContext.cpp
        wrapStageLoadRules.cpp
        wrapStagePopulationMask.cpp
        wrapTimeCode.cpp
        wrapUsdFileFormat.cpp
        wrapUtils.cpp
        wrapVariantSets.cpp
        wrapVersion.cpp 
        wrapZipFile.cpp

    PYMODULE_FILES
        __init__.py
        usdGenSchema.py

    RESOURCE_FILES
        codegenTemplates/__init__.py
        codegenTemplates/api.h
        codegenTemplates/CMakeLists.txt
        codegenTemplates/generatedSchema.classes.txt
        codegenTemplates/generatedSchema.module.h
        codegenTemplates/module.cpp
        codegenTemplates/moduleDeps.cpp
        codegenTemplates/plugInfo.json:no_subst
        codegenTemplates/schema.usda
        codegenTemplates/schemaUserDoc.usda
        codegenTemplates/schemaClass.cpp
        codegenTemplates/schemaClass.h
        codegenTemplates/tokens.cpp
        codegenTemplates/tokens.h
        codegenTemplates/wrapSchemaClass.cpp
        codegenTemplates/wrapTokens.cpp

    DOXYGEN_FILES
        docs/advancedFeatures.md
        docs/authoringEditing.md
        docs/bestPractices.md
        docs/commonIdioms.md
        docs/datatypes.md
        docs/dynamicFileFormat.md
        docs/front.md
        docs/generatingSchemas.md
        docs/instancing.md
        docs/multiThreading.md
        docs/objectModel.md
        docs/propertiesOfSceneDescription.md 
        docs/utf8Overview.md
        docs/valueClips.md
        docs/images/instancing/Instancing_Example.png
        docs/images/instancing/Nested_Instancing_Example.png
        docs/images/instancing/Relationship_Example.png
        docs/images/instancing/Uninstanced_vs_Instanced.png
        examples_usd.cpp
)

if (NOT PXR_ENABLE_PYTHON_SUPPORT)
    message(STATUS "Skipping building usdGenSchema, Python modules required.")
elseif (NOT JINJA2_FOUND)
    message(WARNING "Skipping building usdGenSchema due to missing dependency: Jinja2")
else()
    pxr_python_bin(usdGenSchema
        DEPENDENCIES
            tf
            sdf
            usd
    )
endif()

if (NOT PXR_ENABLE_PYTHON_SUPPORT)
    message(STATUS "Skipping building usdInitSchema, Python modules required.")
elseif (NOT JINJA2_FOUND)
    message(WARNING "Skipping building usdInitSchema due to missing dependency: Jinja2")
else()
    pxr_python_bin(usdInitSchema
        DEPENDENCIES
            usd
    )
endif()

pxr_test_scripts(
    testenv/testUsdAppliedAPISchemas.py
    testenv/testUsdAttributeBlocking.py
    testenv/testUsdAttributeConnections.py
    testenv/testUsdAttributeAssetPathVariableExpressions.py
    testenv/testUsdAttributeQuery.py
    testenv/testUsdBugs.py
    testenv/testUsdBug119633.py
    testenv/testUsdBug141491.py
    testenv/testUsdChangeProcessing.py
    testenv/testUsdClasses.py
    testenv/testUsdCollectionAPI.py
    testenv/testUsdCratePayloadConversionFromVersion07.py
    testenv/testUsdCrateRelocates.py
    testenv/testUsdCrateSpecific.py
    testenv/testUsdCreateProperties.py
    testenv/testUsdDataFormats.py
    testenv/testUsdExternalAssetDependencies.py
    testenv/testUsdErrors.py
    testenv/testUsdFallbackPrimTypes.py
    testenv/testUsdFileFormats.py
    testenv/testUsdFlatten.py
    testenv/testUsdFlatten2.py
    testenv/testUsdFlattenLayerStack.py
    testenv/testUsdFlattenProperties.py
    testenv/testUsdInherits.py
    testenv/testUsdInstanceProxy.py
    testenv/testUsdInstancing.py
    testenv/testUsdLoadUnloadDeepNestedInstancing.py
    testenv/testUsdMetadata.py
    testenv/testUsdModel.py
    testenv/testUsdNamespaceEditor.py
    testenv/testUsdNamespaceEditorDependentEditsBase.py:testUsdNamespaceEditorDependentEditsBase.py
    testenv/testUsdNamespaceEditorDependentEditsBasicClassArcs.py
    testenv/testUsdNamespaceEditorDependentEditsBasicReferencesAndPayloads.py
    testenv/testUsdNamespaceEditorDependentEditsBasicRelocates.py
    testenv/testUsdNamespaceEditorDependentEditsBasicSublayers.py
    testenv/testUsdNamespaceEditorDependentEditsBasicVariants.py
    testenv/testUsdNamespaceEditorDependentEditsSiblingNodeSpecConflicts.py
    testenv/testUsdNamespaceEditorProperties.py
    testenv/testUsdNamespaceEditorTargetPathFixup.py
    testenv/testUsdNotices.py
    testenv/testUsdObjectsChangedNotices.py
    testenv/testUsdObjectsChangedNoticesSublayerOps.py
    testenv/testUsdOpaqueAttributes.py
    testenv/testUsdPathExpressionAttrs.py
    testenv/testUsdPayloads.py
    testenv/testUsdPrimCompositionQuery.py
    testenv/testUsdPrimFlagsPredicate.py
    testenv/testUsdPrimRange.py
    testenv/testUsdPrims.py
    testenv/testUsdReadOutOfBounds.py
    testenv/testUsdReferences.py
    testenv/testUsdRelationships.py
    testenv/testUsdResolveTargetPy.py
    testenv/testUsdSchemaBasePy.py
    testenv/testUsdSchemaRegistry.py
    testenv/testUsdSchemaRegistryThreadedInit.py
    testenv/testUsdSchemaVersioning.py
    testenv/testUsdSpecializes.py
    testenv/testUsdSplines.py
    testenv/testUsdStage.py
    testenv/testUsdStageCache.py
    testenv/testUsdStageDefaultResolver.py
    testenv/testUsdStageLoadUnload.py
    testenv/testUsdStagePopulationMasks.py
    testenv/testUsdTimeCode.py
    testenv/testUsdTimeCodeRepr.py
    testenv/testUsdTimeOffsets.py
    testenv/testUsdTimeSamples.py
    testenv/testUsdTimeValueAuthoring.py
    testenv/testUsdUsdzFileFormat.py
    testenv/testUsdValueClips.py
    testenv/testUsdVariantEditing.py
    testenv/testUsdVariantFallbacks.py
    testenv/testUsdVariants.py
)

pxr_build_test_shared_lib(TestUsdProceduralExternalAssetsFileFormatPlugin
    INSTALL_PREFIX UsdPlugins
    LIBRARIES
        sdf
        tf
        usd
        
    CPPFILES
        testenv/TestUsdProceduralExternalAssetsFileFormatPlugin.cpp
)

pxr_build_test_shared_lib(TestUsdResolverChangedResolver
    INSTALL_PREFIX UsdPlugins
    LIBRARIES
        ar
        plug
        tf

    CPPFILES
        testenv/TestUsdResolverChangedResolver.cpp
)

pxr_build_test(testUsdAttributeBlockingCpp
    LIBRARIES
        tf
        usd
    CPPFILES
        testenv/testUsdAttributeBlocking.cpp
)

pxr_build_test(testUsdAttributeInterpolationCpp
    LIBRARIES
        usd
    CPPFILES
        testenv/testUsdAttributeInterpolationCpp.cpp
)

pxr_build_test(testUsdColorSpaceAPI
    LIBRARIES
        usd
    CPPFILES
        testenv/testUsdColorSpaceAPI.cpp
)

pxr_build_test(testUsdCreateAttributeCpp
    LIBRARIES
        usd
    CPPFILES
        testenv/testUsdCreateAttributeCpp.cpp
)

pxr_build_test(testUsdHardToReach
    LIBRARIES
        usd
    CPPFILES
        testenv/testUsdHardToReach.cpp
)

pxr_build_test(testUsdMetadataCpp
    LIBRARIES
        usd
    CPPFILES
        testenv/testUsdMetadata.cpp
)

pxr_build_test(testUsdResolverChanged
    LIBRARIES
        arch
        plug
        sdf
        tf
        usd
    CPPFILES
        testenv/testUsdResolverChanged.cpp
)

pxr_build_test(testUsdInstancingCpp
    LIBRARIES
        tf
        usd
        sdf
    CPPFILES
        testenv/testUsdInstancingCpp.cpp
)

pxr_build_test(testUsdPrimGetDescendants
    LIBRARIES
        tf
        usd
    CPPFILES
        testenv/testUsdPrimGetDescendants.cpp
)

pxr_build_test(testUsdStageNotification
    LIBRARIES
        usd
    CPPFILES
        testenv/testUsdStageNotification.cpp
)

pxr_build_test(testUsdStageNoPython
    LIBRARIES
        usd
    CPPFILES
        testenv/testUsdStageNoPython.cpp
)

pxr_build_test(testUsdTemplatedIO
    LIBRARIES
        usd
    CPPFILES
        testenv/testUsdTemplatedIO.cpp
)

pxr_build_test(testUsdStageThreading
    LIBRARIES
        arch
        tf
        work
        sdf
        usd
    CPPFILES
        testenv/testUsdStageThreading.cpp
)

pxr_build_test(testUsdThreadedAuthoring
    LIBRARIES
        arch
        tf
        work
        sdf
        usd
        TBB::tbb
    CPPFILES
        testenv/testUsdThreadedAuthoring.cpp
)

pxr_build_test(testUsdTimeCodeStream
    LIBRARIES
        tf
        usd
    CPPFILES
        testenv/testUsdTimeCodeStream.cpp
)

pxr_build_test(testUsdSchemaBase
    LIBRARIES
        usd
    CPPFILES
        testenv/testUsdSchemaBase.cpp
)

pxr_build_test(testUsdSchemaRegistryCpp
    LIBRARIES
        usd
    CPPFILES
        testenv/testUsdSchemaRegistryCpp.cpp
)

pxr_build_test(testUsdResolveTarget
    LIBRARIES
        pcp
        sdf
        tf
        usd
        vt
    CPPFILES
        testenv/testUsdResolveTarget.cpp
)

pxr_build_test(testUsdTimeValueAuthoringCpp
    LIBRARIES
        arch
        plug
        sdf
        tf
        usd
    CPPFILES
        testenv/testUsdTimeValueAuthoring.cpp
)

pxr_build_test(testUsdUsdzBugGHSA01
    LIBRARIES
        ar
        arch
        tf
        sdf
        usd
    CPPFILES
        testenv/testUsdUsdzBugGHSA01.cpp
)

pxr_build_test(testUsdUsdcBugGHSA02
    LIBRARIES
        ar
        arch
        tf
        sdf
        usd
    CPPFILES
    testenv/testUsdUsdcBugGHSA02.cpp
)

pxr_build_test(testUsdSplinesCpp
    LIBRARIES
        tf
        ts
        sdf
        usd
    CPPFILES
        testenv/testUsdSplinesCpp.cpp
)


pxr_install_test_dir(
    SRC testenv/testUsdAppliedAPISchemas
    DEST testUsdAppliedAPISchemas
)

pxr_install_test_dir(
    SRC testenv/testUsdAppliedAPISchemas
    DEST testUsdAppliedAPISchemas_AutoApplyDisabled
)

pxr_install_test_dir(
    SRC testenv/testUsdStageThreading
    DEST testUsdStageThreading
)

pxr_install_test_dir(
    SRC testenv/testUsdCratePayloadConversionFromVersion07.testenv
    DEST testUsdCratePayloadConversionFromVersion07
)

pxr_install_test_dir(
    SRC testenv/testUsdCrateRelocates.testenv
    DEST testUsdCrateRelocates
)

pxr_install_test_dir(
    SRC testenv/testUsdExternalAssetDependencies.testenv
    DEST testUsdExternalAssetDependencies
)

pxr_install_test_dir(
    SRC testenv/testUsdFallbackPrimTypes
    DEST testUsdFallbackPrimTypes
)

pxr_install_test_dir(
    SRC testenv/testUsdFlatten
    DEST testUsdFlatten
)

pxr_install_test_dir(
    SRC testenv/testUsdFlatten2
    DEST testUsdFlatten2
)

pxr_install_test_dir(
    SRC testenv/testUsdFlatten2
    DEST testUsdFlatten3
)

pxr_install_test_dir(
    SRC testenv/testUsdFlatten2
    DEST testUsdFlatten4
)

pxr_install_test_dir(
    SRC testenv/testUsdFlattenLayerStack
    DEST testUsdFlattenLayerStack
)

pxr_install_test_dir(
    SRC testenv/testUsdFlattenProperties
    DEST testUsdFlattenProperties
)

pxr_install_test_dir(
    SRC testenv/testUsdFileFormats
    DEST testUsdFileFormats
)

pxr_install_test_dir(
    SRC testenv/testUsdFileFormats
    DEST testUsdFileFormats_asset
)

pxr_install_test_dir(
    SRC testenv/testUsdFileFormats
    DEST testUsdFileFormats_pread
)

pxr_install_test_dir(
    SRC testenv/testUsdVariantEditing.testenv
    DEST testUsdEditVariantEditing
)

pxr_install_test_dir(
    SRC testenv/testUsdInstancing
    DEST testUsdInstancing
)

pxr_install_test_dir(
    SRC testenv/testUsdInstanceProxy
    DEST testUsdInstanceProxy
)

pxr_install_test_dir(
    SRC testenv/testUsdMetadata
    DEST testUsdMetadata
)

pxr_install_test_dir(
    SRC testenv/testUsdReadOutOfBounds
    DEST testUsdReadOutOfBounds
)

pxr_install_test_dir(
    SRC testenv/testUsdResolverChanged
    DEST testUsdResolverChanged
)

pxr_install_test_dir(
    SRC testenv/testUsdResolveTarget.testenv
    DEST testUsdResolveTarget
)

pxr_install_test_dir(
    SRC testenv/testUsdResolveTarget.testenv
    DEST testUsdResolveTargetPy
)

pxr_install_test_dir(
    SRC testenv/testUsdNotices.testenv
    DEST testUsdNotices
)

pxr_install_test_dir(
    SRC testenv/testUsdNamespaceEditor
    DEST testUsdNamespaceEditor
)

pxr_install_test_dir(
    SRC testenv/testUsdNamespaceEditorProperties
    DEST testUsdNamespaceEditorProperties
)

pxr_install_test_dir(
    SRC testenv/testUsdNamespaceEditorTargetPathFixup
    DEST testUsdNamespaceEditorTargetPathFixup
)

pxr_install_test_dir(
    SRC testenv/testUsdObjectsChangedNoticesSublayerOps
    DEST testUsdObjectsChangedNoticesSublayerOps
)

pxr_install_test_dir(
    SRC testenv/testUsdPrims.testenv
    DEST testUsdPrims
)

pxr_install_test_dir(
    SRC testenv/testUsdPrimCompositionQuery.testenv
    DEST testUsdPrimCompositionQuery
)

pxr_install_test_dir(
    SRC testenv/testUsdPrimGetDescendants.testenv
    DEST testUsdPrimGetDescendants
)

pxr_install_test_dir(
    SRC testenv/testUsdInstancingCpp.testenv
    DEST testUsdInstancingCpp
)

pxr_install_test_dir(
    SRC testenv/testUsdVariantEditing.testenv
    DEST testUsdVariantEditing
)

pxr_install_test_dir(
    SRC testenv/testUsdVariantFallbacks.testenv
    DEST testUsdVariantFallbacks
)

pxr_install_test_dir(
    SRC testenv/testUsdGenSchema
    DEST testUsdGenSchema
)

pxr_install_test_dir(
    SRC testenv/testUsdSchemaRegistry
    DEST testUsdSchemaRegistry
)

pxr_install_test_dir(
    SRC testenv/testUsdSchemaRegistry
    DEST testUsdSchemaRegistryCpp
)

pxr_install_test_dir(
    SRC testenv/testUsdSchemaVersioning
    DEST testUsdSchemaVersioning
)

pxr_install_test_dir(
    SRC testenv/testUsdTimeValueAuthoring
    DEST testUsdTimeValueAuthoring
)

pxr_install_test_dir(
    SRC testenv/testUsdTimeValueAuthoring
    DEST testUsdTimeValueAuthoringCpp
)

pxr_install_test_dir(
    SRC testenv/testUsdBug119633.testenv
    DEST testUsdBug119633
)

pxr_install_test_dir(
    SRC testenv/testUsdBug141491.testenv
    DEST testUsdBug141491
)

pxr_install_test_dir(
    SRC testenv/testUsdLoadUnloadDeepNestedInstancing.testenv
    DEST testUsdLoadUnloadDeepNestedInstancing
)

pxr_install_test_dir(
    SRC testenv/testUsdStageNoPython.testenv
    DEST testUsdStageNoPython
)

pxr_install_test_dir(
    SRC testenv/testUsdPrimRange
    DEST testUsdPrimRange
)

pxr_install_test_dir(
    SRC testenv/testUsdValueClips
    DEST testUsdValueClips
)

pxr_install_test_dir(
    SRC testenv/testUsdVariants
    DEST testUsdVariants
)

pxr_install_test_dir(
    SRC testenv/testUsdCollectionAPI
    DEST testUsdCollectionAPI
)

pxr_install_test_dir(
    SRC testenv/testUsdUsdzFileFormat
    DEST testUsdUsdzFileFormat
)

pxr_install_test_dir(
    SRC testenv/testUsdUsdzFileFormat
    DEST testUsdUsdzFileFormat_asset
)

pxr_install_test_dir(
    SRC testenv/testUsdUsdzFileFormat
    DEST testUsdUsdzFileFormat_pread
)

pxr_install_test_dir(
    SRC testenv/testUsdUsdzBugGHSA01.testenv
    DEST testUsdUsdzBugGHSA01
)

pxr_install_test_dir(
    SRC testenv/testUsdUsdcBugGHSA02.testenv
    DEST testUsdUsdcBugGHSA02
)

pxr_register_test(testUsdAppliedAPISchemas
    PYTHON
    COMMAND "${CMAKE_INSTALL_PREFIX}/tests/testUsdAppliedAPISchemas"
    EXPECTED_RETURN_CODE 0
)

pxr_register_test(testUsdAppliedAPISchemas_AutoApplyDisabled
    PYTHON
    COMMAND "${CMAKE_INSTALL_PREFIX}/tests/testUsdAppliedAPISchemas"
    EXPECTED_RETURN_CODE 0
    ENV
        USD_DISABLE_AUTO_APPLY_API_SCHEMAS=1
)

pxr_register_test(testUsdAttributeBlocking
    PYTHON
    COMMAND "${CMAKE_INSTALL_PREFIX}/tests/testUsdAttributeBlocking"
    EXPECTED_RETURN_CODE 0
)

pxr_register_test(testUsdAttributeBlockingCpp
    COMMAND "${CMAKE_INSTALL_PREFIX}/tests/testUsdAttributeBlockingCpp"
    EXPECTED_RETURN_CODE 0
)

pxr_register_test(testUsdAttributeConnections
    PYTHON
    COMMAND "${CMAKE_INSTALL_PREFIX}/tests/testUsdAttributeConnections"
    EXPECTED_RETURN_CODE 0
)

pxr_register_test(testUsdAttributeAssetPathVariableExpressions
    PYTHON
    COMMAND "${CMAKE_INSTALL_PREFIX}/tests/testUsdAttributeAssetPathVariableExpressions"
    EXPECTED_RETURN_CODE 0
)

pxr_register_test(testUsdAttributeQuery
    PYTHON
    COMMAND "${CMAKE_INSTALL_PREFIX}/tests/testUsdAttributeQuery"
    EXPECTED_RETURN_CODE 0
)

pxr_register_test(testUsdErrors
    PYTHON
    COMMAND "${CMAKE_INSTALL_PREFIX}/tests/testUsdErrors"
    ENV TF_FATAL_THROW=0
    EXPECTED_RETURN_CODE 0
)

pxr_register_test(testUsdValueClips
    PYTHON
    COMMAND "${CMAKE_INSTALL_PREFIX}/tests/testUsdValueClips"
    EXPECTED_RETURN_CODE 0
)

pxr_register_test(testUsdVariantEditing
    PYTHON
    COMMAND "${CMAKE_INSTALL_PREFIX}/tests/testUsdVariantEditing"
    EXPECTED_RETURN_CODE 0
)

pxr_register_test(testUsdVariantFallbacks
    PYTHON
    COMMAND "${CMAKE_INSTALL_PREFIX}/tests/testUsdVariantFallbacks"
    EXPECTED_RETURN_CODE 0
)

pxr_register_test(testUsdVariants
    PYTHON
    COMMAND "${CMAKE_INSTALL_PREFIX}/tests/testUsdVariants"
    EXPECTED_RETURN_CODE 0
)

pxr_register_test(testUsdAttributeInterpolationCpp
    COMMAND "${CMAKE_INSTALL_PREFIX}/tests/testUsdAttributeInterpolationCpp"
    EXPECTED_RETURN_CODE 0
)

pxr_register_test(testUsdBugs
    PYTHON
    COMMAND "${CMAKE_INSTALL_PREFIX}/tests/testUsdBugs"
    EXPECTED_RETURN_CODE 0
)

pxr_register_test(testUsdBug119633
    PYTHON
    COMMAND "${CMAKE_INSTALL_PREFIX}/tests/testUsdBug119633"
    EXPECTED_RETURN_CODE 0
)

pxr_register_test(testUsdBug141491
    PYTHON
    COMMAND "${CMAKE_INSTALL_PREFIX}/tests/testUsdBug141491"
    EXPECTED_RETURN_CODE 0
)

pxr_register_test(testUsdChangeProcessing
    PYTHON
    COMMAND "${CMAKE_INSTALL_PREFIX}/tests/testUsdChangeProcessing"
    EXPECTED_RETURN_CODE 0
)

pxr_register_test(testUsdCollectionAPI
    PYTHON
    COMMAND "${CMAKE_INSTALL_PREFIX}/tests/testUsdCollectionAPI"
    EXPECTED_RETURN_CODE 0
)

pxr_register_test(testUsdClasses
    PYTHON
    COMMAND "${CMAKE_INSTALL_PREFIX}/tests/testUsdClasses"
    EXPECTED_RETURN_CODE 0
)

pxr_register_test(testUsdCreateProperties
    PYTHON
    COMMAND "${CMAKE_INSTALL_PREFIX}/tests/testUsdCreateProperties"
    EXPECTED_RETURN_CODE 0
)

pxr_register_test(testUsdCreateAttributeCpp
    COMMAND "${CMAKE_INSTALL_PREFIX}/tests/testUsdCreateAttributeCpp"
    EXPECTED_RETURN_CODE 0
)

pxr_register_test(testUsdExternalAssetDependencies
    PYTHON
    COMMAND "${CMAKE_INSTALL_PREFIX}/tests/testUsdExternalAssetDependencies"
    EXPECTED_RETURN_CODE 0  
)

pxr_register_test(testUsdFallbackPrimTypes
    PYTHON
    COMMAND "${CMAKE_INSTALL_PREFIX}/tests/testUsdFallbackPrimTypes"
    EXPECTED_RETURN_CODE 0
)

pxr_register_test(testUsdHardToReach
    COMMAND "${CMAKE_INSTALL_PREFIX}/tests/testUsdHardToReach"
    EXPECTED_RETURN_CODE 0
)

pxr_register_test(testUsdInstancing
    PYTHON
    COMMAND "${CMAKE_INSTALL_PREFIX}/tests/testUsdInstancing"
    ENV
        USD_ASSIGN_PROTOTYPES_DETERMINISTICALLY=1
)

pxr_register_test(testUsdInstanceProxy
    PYTHON
    COMMAND "${CMAKE_INSTALL_PREFIX}/tests/testUsdInstanceProxy"
    EXPECTED_RETURN_CODE 0
)

pxr_register_test(testUsdDataFormats
    PYTHON
    COMMAND "${CMAKE_INSTALL_PREFIX}/tests/testUsdDataFormats"
    EXPECTED_RETURN_CODE 0
)

pxr_register_test(testUsdLoadUnloadDeepNestedInstancing
    PYTHON
    COMMAND "${CMAKE_INSTALL_PREFIX}/tests/testUsdLoadUnloadDeepNestedInstancing"
    EXPECTED_RETURN_CODE 0
)

pxr_register_test(testUsdTimeSamples
    PYTHON
    COMMAND "${CMAKE_INSTALL_PREFIX}/tests/testUsdTimeSamples"
    EXPECTED_RETURN_CODE 0
)

pxr_register_test(testUsdReferences
    PYTHON
    COMMAND "${CMAKE_INSTALL_PREFIX}/tests/testUsdReferences"
    EXPECTED_RETURN_CODE 0
)

pxr_register_test(testUsdTemplatedIO
    COMMAND "${CMAKE_INSTALL_PREFIX}/tests/testUsdTemplatedIO"
    EXPECTED_RETURN_CODE 0
)

pxr_register_test(testUsdPrimCompositionQuery
    PYTHON
    COMMAND "${CMAKE_INSTALL_PREFIX}/tests/testUsdPrimCompositionQuery"
    EXPECTED_RETURN_CODE 0
)

pxr_register_test(testUsdPrimGetDescendants
    COMMAND "${CMAKE_INSTALL_PREFIX}/tests/testUsdPrimGetDescendants"
    EXPECTED_RETURN_CODE 0
)

pxr_register_test(testUsdInstancingCpp
    COMMAND "${CMAKE_INSTALL_PREFIX}/tests/testUsdInstancingCpp"
    EXPECTED_RETURN_CODE 0
    ENV
        USD_ASSIGN_PROTOTYPES_DETERMINISTICALLY=1
)

pxr_register_test(testUsdPrims
    PYTHON
    COMMAND "${CMAKE_INSTALL_PREFIX}/tests/testUsdPrims"
    EXPECTED_RETURN_CODE 0
)

pxr_register_test(testUsdFileFormats
    PYTHON
    COMMAND "${CMAKE_INSTALL_PREFIX}/tests/testUsdFileFormats"
    EXPECTED_RETURN_CODE 0
)

pxr_register_test(testUsdFileFormats_asset
    PYTHON
    COMMAND "${CMAKE_INSTALL_PREFIX}/tests/testUsdFileFormats"
    ENV
        USDC_USE_ASSET=1
    EXPECTED_RETURN_CODE 0
)

pxr_register_test(testUsdFileFormats_pread
    PYTHON
    COMMAND "${CMAKE_INSTALL_PREFIX}/tests/testUsdFileFormats"
    ENV
        USDC_USE_PREAD=1
    EXPECTED_RETURN_CODE 0
)

pxr_register_test(testUsdMetadataCpp
    COMMAND "${CMAKE_INSTALL_PREFIX}/tests/testUsdMetadataCpp"
    EXPECTED_RETURN_CODE 0
)

#The following tests rely on plugins built specifically for them.
#The build system currently doesn't support test-specific plugins
#for static library builds, so we only enable these tests for
#shared library builds.
if (BUILD_SHARED_LIBS)
    pxr_register_test(testUsdResolverChanged
        COMMAND "${CMAKE_INSTALL_PREFIX}/tests/testUsdResolverChanged"
        EXPECTED_RETURN_CODE 0
    )
endif()

pxr_register_test(testUsdStageNoPython
    COMMAND "${CMAKE_INSTALL_PREFIX}/tests/testUsdStageNoPython"
    EXPECTED_RETURN_CODE 0
)
pxr_register_test(testUsdStageNotification
    COMMAND "${CMAKE_INSTALL_PREFIX}/tests/testUsdStageNotification"
    EXPECTED_RETURN_CODE 0
)
pxr_register_test(testUsdStageThreading
    COMMAND "${CMAKE_INSTALL_PREFIX}/tests/testUsdStageThreading"
    EXPECTED_RETURN_CODE 0
)
pxr_register_test(testUsdThreadedAuthoring
    COMMAND "${CMAKE_INSTALL_PREFIX}/tests/testUsdThreadedAuthoring"
    EXPECTED_RETURN_CODE 0
)
pxr_register_test(testUsdTimeCodeStream
    COMMAND "${CMAKE_INSTALL_PREFIX}/tests/testUsdTimeCodeStream"
    EXPECTED_RETURN_CODE 0
)

pxr_register_test(testUsdStage
    PYTHON
    COMMAND "${CMAKE_INSTALL_PREFIX}/tests/testUsdStage"
    EXPECTED_RETURN_CODE 0
)

pxr_register_test(testUsdStageCache
    PYTHON
    COMMAND "${CMAKE_INSTALL_PREFIX}/tests/testUsdStageCache"
    EXPECTED_RETURN_CODE 0
)

pxr_register_test(testUsdStagePopulationMasks
    PYTHON
    COMMAND "${CMAKE_INSTALL_PREFIX}/tests/testUsdStagePopulationMasks"
    EXPECTED_RETURN_CODE 0
)

pxr_register_test(testUsdTimeCodeRepr
    PYTHON
    COMMAND "${CMAKE_INSTALL_PREFIX}/tests/testUsdTimeCodeRepr"
    EXPECTED_RETURN_CODE 0
)

pxr_register_test(testUsdTimeCode
    PYTHON
    COMMAND "${CMAKE_INSTALL_PREFIX}/tests/testUsdTimeCode"
    EXPECTED_RETURN_CODE 0
)

pxr_register_test(testUsdTimeOffsets
    PYTHON
    COMMAND "${CMAKE_INSTALL_PREFIX}/tests/testUsdTimeOffsets"
    EXPECTED_RETURN_CODE 0
)

pxr_register_test(testUsdPrimRange
    PYTHON
    COMMAND "${CMAKE_INSTALL_PREFIX}/tests/testUsdPrimRange"
    EXPECTED_RETURN_CODE 0
)

pxr_register_test(testUsdStageDefaultResolver
    PYTHON
    COMMAND "${CMAKE_INSTALL_PREFIX}/tests/testUsdStageDefaultResolver"
    EXPECTED_RETURN_CODE 0
)

pxr_register_test(testUsdStageLoadUnload
    PYTHON
    COMMAND "${CMAKE_INSTALL_PREFIX}/tests/testUsdStageLoadUnload"
    EXPECTED_RETURN_CODE 0
)

pxr_register_test(testUsdPayloads
    PYTHON
    COMMAND "${CMAKE_INSTALL_PREFIX}/tests/testUsdPayloads"
    EXPECTED_RETURN_CODE 0
)

pxr_register_test(testUsdPathExpressionAttrs
    PYTHON
    COMMAND "${CMAKE_INSTALL_PREFIX}/tests/testUsdPathExpressionAttrs"
    EXPECTED_RETURN_CODE 0
)

pxr_register_test(testUsdCratePayloadConversionFromVersion07
    PYTHON
    COMMAND "${CMAKE_INSTALL_PREFIX}/tests/testUsdCratePayloadConversionFromVersion07"
    EXPECTED_RETURN_CODE 0
)

pxr_register_test(testUsdCrateRelocates
    PYTHON
    COMMAND "${CMAKE_INSTALL_PREFIX}/tests/testUsdCrateRelocates"
    EXPECTED_RETURN_CODE 0
)

pxr_register_test(testUsdCrateSpecific
    PYTHON
    COMMAND "${CMAKE_INSTALL_PREFIX}/tests/testUsdCrateSpecific"
    EXPECTED_RETURN_CODE 0
)

pxr_register_test(testUsdInherits
    PYTHON
    COMMAND "${CMAKE_INSTALL_PREFIX}/tests/testUsdInherits"
    EXPECTED_RETURN_CODE 0
)

pxr_register_test(testUsdModel
    PYTHON
    COMMAND "${CMAKE_INSTALL_PREFIX}/tests/testUsdModel"
    EXPECTED_RETURN_CODE 0
)

pxr_register_test(testUsdResolveTarget
    COMMAND "${CMAKE_INSTALL_PREFIX}/tests/testUsdResolveTarget"
    EXPECTED_RETURN_CODE 0
)

pxr_register_test(testUsdResolveTargetPy
    PYTHON
    COMMAND "${CMAKE_INSTALL_PREFIX}/tests/testUsdResolveTargetPy"
    EXPECTED_RETURN_CODE 0
)

pxr_register_test(testUsdSpecializes
    PYTHON
    COMMAND "${CMAKE_INSTALL_PREFIX}/tests/testUsdSpecializes"
    EXPECTED_RETURN_CODE 0
)

pxr_register_test(testUsdSplines
    PYTHON
    COMMAND "${CMAKE_INSTALL_PREFIX}/tests/testUsdSplines"
    EXPECTED_RETURN_CODE 0
)

pxr_register_test(testUsdMetadata
    PYTHON
    COMMAND "${CMAKE_INSTALL_PREFIX}/tests/testUsdMetadata"
    EXPECTED_RETURN_CODE 0
)

pxr_register_test(testUsdFlatten
    PYTHON
    COMMAND "${CMAKE_INSTALL_PREFIX}/tests/testUsdFlatten"
    EXPECTED_RETURN_CODE 0
)

pxr_register_test(testUsdFlatten2
    PYTHON
    COMMAND "${CMAKE_INSTALL_PREFIX}/tests/testUsdFlatten2 BasicInstancingAndVariants/root.usda"
    STDOUT_REDIRECT flat_BasicInstancingAndVariants.usda
    DIFF_COMPARE flat_BasicInstancingAndVariants.usda
    EXPECTED_RETURN_CODE 0
    ENV
        USD_ASSIGN_PROTOTYPES_DETERMINISTICALLY=1
)

pxr_register_test(testUsdFlatten3
    PYTHON
    COMMAND "${CMAKE_INSTALL_PREFIX}/tests/testUsdFlatten2 BasicInstancing/root.usda"
    STDOUT_REDIRECT flat_BasicInstancing.usda
    DIFF_COMPARE flat_BasicInstancing.usda
    EXPECTED_RETURN_CODE 0
    ENV
        USD_ASSIGN_PROTOTYPES_DETERMINISTICALLY=1
)

pxr_register_test(testUsdFlatten4
    PYTHON
    COMMAND "${CMAKE_INSTALL_PREFIX}/tests/testUsdFlatten2 BasicInstancingAndNestedInstances/root.usda"
    STDOUT_REDIRECT flat_BasicInstancingAndNestedInstances.usda
    DIFF_COMPARE flat_BasicInstancingAndNestedInstances.usda
    EXPECTED_RETURN_CODE 0
    ENV
        USD_ASSIGN_PROTOTYPES_DETERMINISTICALLY=1
)

pxr_register_test(testUsdFlattenLayerStack
    PYTHON
    COMMAND "${CMAKE_INSTALL_PREFIX}/tests/testUsdFlattenLayerStack"
    EXPECTED_RETURN_CODE 0
)

pxr_register_test(testUsdNotices
    PYTHON
    COMMAND "${CMAKE_INSTALL_PREFIX}/tests/testUsdNotices"
    EXPECTED_RETURN_CODE 0
)

pxr_register_test(testUsdObjectsChangedNotices
    PYTHON
    COMMAND "${CMAKE_INSTALL_PREFIX}/tests/testUsdObjectsChangedNotices"
    EXPECTED_RETURN_CODE 0
)

pxr_register_test(testUsdObjectsChangedNoticesSublayerOps
    PYTHON
    COMMAND "${CMAKE_INSTALL_PREFIX}/tests/testUsdObjectsChangedNoticesSublayerOps"
    EXPECTED_RETURN_CODE 0
)

pxr_register_test(testUsdNamespaceEditor
    PYTHON
    COMMAND "${CMAKE_INSTALL_PREFIX}/tests/testUsdNamespaceEditor"
    EXPECTED_RETURN_CODE 0
)

pxr_register_test(testUsdNamespaceEditorDependentEditsBasicClassArcs
    PYTHON
    COMMAND "${CMAKE_INSTALL_PREFIX}/tests/testUsdNamespaceEditorDependentEditsBasicClassArcs"
    EXPECTED_RETURN_CODE 0
)

pxr_register_test(testUsdNamespaceEditorDependentEditsBasicReferencesAndPayloads
    PYTHON
    COMMAND "${CMAKE_INSTALL_PREFIX}/tests/testUsdNamespaceEditorDependentEditsBasicReferencesAndPayloads"
    EXPECTED_RETURN_CODE 0
)

pxr_register_test(testUsdNamespaceEditorDependentEditsBasicRelocates
    PYTHON
    COMMAND "${CMAKE_INSTALL_PREFIX}/tests/testUsdNamespaceEditorDependentEditsBasicRelocates"
    EXPECTED_RETURN_CODE 0
)

pxr_register_test(testUsdNamespaceEditorDependentEditsBasicSublayers
    PYTHON
    COMMAND "${CMAKE_INSTALL_PREFIX}/tests/testUsdNamespaceEditorDependentEditsBasicSublayers"
    EXPECTED_RETURN_CODE 0
)

pxr_register_test(testUsdNamespaceEditorDependentEditsBasicVariants
    PYTHON
    COMMAND "${CMAKE_INSTALL_PREFIX}/tests/testUsdNamespaceEditorDependentEditsBasicVariants"
    EXPECTED_RETURN_CODE 0
)

pxr_register_test(testUsdNamespaceEditorDependentEditsSiblingNodeSpecConflicts
    PYTHON
    COMMAND "${CMAKE_INSTALL_PREFIX}/tests/testUsdNamespaceEditorDependentEditsSiblingNodeSpecConflicts"
    EXPECTED_RETURN_CODE 0
)

pxr_register_test(testUsdNamespaceEditorProperties
    PYTHON
    COMMAND "${CMAKE_INSTALL_PREFIX}/tests/testUsdNamespaceEditorProperties"
    EXPECTED_RETURN_CODE 0
)

pxr_register_test(testUsdNamespaceEditorTargetPathFixup
    PYTHON
    COMMAND "${CMAKE_INSTALL_PREFIX}/tests/testUsdNamespaceEditorTargetPathFixup"
    EXPECTED_RETURN_CODE 0
)

pxr_register_test(testUsdReadOutOfBounds
    PYTHON
    COMMAND "${CMAKE_INSTALL_PREFIX}/tests/testUsdReadOutOfBounds"
    EXPECTED_RETURN_CODE 0
)

pxr_register_test(testUsdRelationships
    PYTHON
    COMMAND "${CMAKE_INSTALL_PREFIX}/tests/testUsdRelationships"
    EXPECTED_RETURN_CODE 0
)

pxr_register_test(testUsdSchemaBase
    COMMAND "${CMAKE_INSTALL_PREFIX}/tests/testUsdSchemaBase"
    EXPECTED_RETURN_CODE 0
)

pxr_register_test(testUsdSchemaBasePy
    PYTHON
    COMMAND "${CMAKE_INSTALL_PREFIX}/tests/testUsdSchemaBasePy"
    EXPECTED_RETURN_CODE 0
)

pxr_register_test(testUsdSchemaRegistry
    PYTHON
    COMMAND "${CMAKE_INSTALL_PREFIX}/tests/testUsdSchemaRegistry"
    EXPECTED_RETURN_CODE 0
)

pxr_register_test(testUsdSchemaRegistryCpp
    COMMAND "${CMAKE_INSTALL_PREFIX}/tests/testUsdSchemaRegistryCpp"
    EXPECTED_RETURN_CODE 0
)

pxr_register_test(testUsdSchemaRegistryThreadedInit
    PYTHON
    COMMAND "${CMAKE_INSTALL_PREFIX}/tests/testUsdSchemaRegistryThreadedInit"
    EXPECTED_RETURN_CODE 0
)

pxr_register_test(testUsdSchemaVersioning
    PYTHON
    COMMAND "${CMAKE_INSTALL_PREFIX}/tests/testUsdSchemaVersioning"
    EXPECTED_RETURN_CODE 0
)

pxr_register_test(testUsdTimeValueAuthoring
    PYTHON
    COMMAND "${CMAKE_INSTALL_PREFIX}/tests/testUsdTimeValueAuthoring"
    EXPECTED_RETURN_CODE 0
)

pxr_register_test(testUsdTimeValueAuthoringCpp
    COMMAND "${CMAKE_INSTALL_PREFIX}/tests/testUsdTimeValueAuthoringCpp"
    EXPECTED_RETURN_CODE 0
)

pxr_register_test(testUsdSplinesCpp
    COMMAND "${CMAKE_INSTALL_PREFIX}/tests/testUsdSplinesCpp"
    EXPECTED_RETURN_CODE 0
)

pxr_register_test(testUsdUsdzFileFormat
    PYTHON
    COMMAND "${CMAKE_INSTALL_PREFIX}/tests/testUsdUsdzFileFormat"
    EXPECTED_RETURN_CODE 0
)

pxr_register_test(testUsdUsdzFileFormat_asset
    PYTHON
    COMMAND "${CMAKE_INSTALL_PREFIX}/tests/testUsdUsdzFileFormat"
    ENV
        USDC_USE_ASSET=1
    EXPECTED_RETURN_CODE 0
)

pxr_register_test(testUsdUsdzFileFormat_pread
    PYTHON
    COMMAND "${CMAKE_INSTALL_PREFIX}/tests/testUsdUsdzFileFormat"
    ENV
        USDC_USE_PREAD=1
    EXPECTED_RETURN_CODE 0
)

pxr_register_test(testUsdOpaqueAttributes
    PYTHON
    COMMAND "${CMAKE_INSTALL_PREFIX}/tests/testUsdOpaqueAttributes"
    EXPECTED_RETURN_CODE 0
)

pxr_register_test(testUsdUsdzBugGHSA01
    COMMAND "${CMAKE_INSTALL_PREFIX}/tests/testUsdUsdzBugGHSA01"
    EXPECTED_RETURN_CODE 0
)

pxr_register_test(testUsdUsdcBugGHSA02
    COMMAND "${CMAKE_INSTALL_PREFIX}/tests/testUsdUsdcBugGHSA02"
    EXPECTED_RETURN_CODE 0
)
  
