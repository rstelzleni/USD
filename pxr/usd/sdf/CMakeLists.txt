set(PXR_PREFIX pxr/usd)
set(PXR_PACKAGE sdf)

pxr_library(sdf
    LIBRARIES
        arch
        tf
        gf
        pegtl
        trace
        ts
        vt
        work
        ar
        TBB::tbb

    PUBLIC_CLASSES
        abstractData
        allowed
        assetPath
        attributeSpec
        changeBlock
        changeList
        children
        childrenPolicies
        childrenProxy
        childrenUtils
        childrenView
        cleanupEnabler
        copyUtils
        crateInfo
        data
        declareHandles
        fileFormat
        identity
        layer
        layerOffset
        layerStateDelegate
        layerTree
        layerUtils
        listProxy
        listEditor
        listEditorProxy
        listOp
        mapEditProxy
        mapEditor
        namespaceEdit
        notice
        opaqueValue
        path
        pathExpression
        pathExpressionEval
        pathNode
        pathPattern
        pathTable
        payload
        pool
        predicateExpression
        predicateLibrary
        primSpec
        propertySpec
        proxyPolicies
        proxyTypes
        pseudoRootSpec
        reference
        relationshipSpec
        schema
        site
        siteUtils
        spec
        specType
        textFileFormat
        timeCode
        tokens
        types
        usdFileFormat
        usdaFileFormat
        usdcFileFormat
        usdzFileFormat
        valueTypeName
        variableExpression
        variantSetSpec
        variantSpec
        zipFile
   
    PYTHON_PUBLIC_CLASSES
        pyChildrenProxy
        pyChildrenView
        pyListEditorProxy
        pyListOp
        pyListProxy
        pyMapEditProxy
        pySpec
        pyUtils

    PUBLIC_HEADERS
        api.h
        accessorHelpers.h
        declareSpec.h
        layerHints.h
        pathPatternParser.h
        predicateExpressionParser.h
        predicateProgram.h
        schemaTypeRegistration.h

    PRIVATE_CLASSES
        assetPathResolver
        changeManager
        cleanupTracker
        connectionListEditor
        crateData
        crateFile
        debugCodes
        fileFormatRegistry
        fileIO
        fileIO_Common
        integerCoding
        layerRegistry
        listOpListEditor
        parserHelpers
        parserValueContext
        pathParser
        subLayerListEditor
        textFileFormatParser
        textParserContext
        textParserHelpers
        usdzResolver
        valueTypeRegistry
        variableExpressionImpl
        variableExpressionParser
        vectorListEditor

    PRIVATE_HEADERS
        crateDataTypes.h
        crateValueInliners.h
        instantiatePool.h
        shared.h
        valueTypePrivate.h

    PYMODULE_CPPFILES
        module.cpp
        wrapArrayAssetPath.cpp
        wrapArrayPath.cpp
        wrapArrayPathExpression.cpp
        wrapArrayTimeCode.cpp
        wrapAssetPath.cpp
        wrapAttributeSpec.cpp
        wrapChangeBlock.cpp
        wrapCleanupEnabler.cpp
        wrapCopyUtils.cpp
        wrapCrateInfo.cpp
        wrapFileFormat.cpp
        wrapLayer.cpp
        wrapLayerOffset.cpp
        wrapLayerTree.cpp
        wrapNamespaceEdit.cpp
        wrapNotice.cpp
        wrapOpaqueValue.cpp
        wrapPath.cpp
        wrapPathExpression.cpp
        wrapPathPattern.cpp
        wrapPayload.cpp
        wrapPredicateExpression.cpp
        wrapPredicateFunctionResult.cpp
        wrapPrimSpec.cpp
        wrapPropertySpec.cpp
        wrapPseudoRootSpec.cpp
        wrapRelationshipSpec.cpp
        wrapReference.cpp
        wrapSpec.cpp
        wrapVariableExpression.cpp
        wrapTimeCode.cpp
        wrapTypes.cpp
        wrapUsdFileFormat.cpp
        wrapValueTypeName.cpp
        wrapVariantSpec.cpp
        wrapVariantSetSpec.cpp
        wrapZipFile.cpp

    PYMODULE_FILES 
        __init__.py

    RESOURCE_FILES
        plugInfo.json

    DOXYGEN_FILES
        overview.dox
        doxygen/fileFormatPlugin.dox
        doxygen/variableExpressions.dox
)

pxr_test_scripts(
    testenv/testSdfAssetPath.py
    testenv/testSdfAttribute.py
    testenv/testSdfAttributeBlocking.py
    testenv/testSdfBatchNamespaceEdit.py
    testenv/testSdfFileFormatCapabilities.py
    testenv/testSdfCopyUtils.py
    testenv/testSdfCustomLayerData.py
    testenv/testSdfDetachedLayer.py
    testenv/testSdfFileFormat.py
    testenv/testSdfLayer.py
    testenv/testSdfLayerMuting.py
    testenv/testSdfLegacyFileFormat.py
    testenv/testSdfListOp.py
    testenv/testSdfParsing.py
    testenv/testSdfPath.py
    testenv/testSdfPath2.py
    testenv/testSdfPath2Construct.py
    testenv/testSdfPathExpression.py
    testenv/testSdfPathExpressionArray.py
    testenv/testSdfPayload.py
    testenv/testSdfPrim.py
    testenv/testSdfReference.py
    testenv/testSdfRelocates.py
    testenv/testSdfSpecHash.py
    testenv/testSdfTargetFileFormat.py
    testenv/testSdfTimeCode.py
    testenv/testSdfTypes.py
    testenv/testSdfVariableExpression.py
    testenv/testSdfVariants.py
    testenv/testSdfZipFile.py
)

pxr_build_test_shared_lib(TestSdfFileFormatCapabilities
    INSTALL_PREFIX SdfPlugins
    LIBRARIES
        sdf
        tf

    CPPFILES
        testenv/TestSdfFileFormatCapabilities.cpp
)

pxr_build_test_shared_lib(TestSdfLayerExceptionHandling
    INSTALL_PREFIX SdfPlugins
    LIBRARIES
        sdf
        tf

    CPPFILES
        testenv/TestSdfLayerExceptionHandling.cpp
)

pxr_build_test_shared_lib(TestSdfNoAssetFileFormat
    INSTALL_PREFIX SdfPlugins
    LIBRARIES
        sdf
        tf

    CPPFILES
        testenv/TestSdfNoAssetFileFormat.cpp
)

pxr_build_test_shared_lib(TestSdfStreamingFileFormat
    INSTALL_PREFIX SdfPlugins
    LIBRARIES
        sdf
        tf

    CPPFILES
        testenv/TestSdfStreamingFileFormat.cpp
)

pxr_build_test_shared_lib(TestSdfTargetFileFormat
    INSTALL_PREFIX SdfPlugins
    LIBRARIES
        sdf
        tf

    CPPFILES
        testenv/TestSdfTargetFileFormat.cpp
)

pxr_build_test_shared_lib(TestSdfResolver
    INSTALL_PREFIX SdfPlugins
    LIBRARIES
        ar
        sdf
        tf

    CPPFILES
        testenv/TestSdfResolver.cpp
)

pxr_build_test(testSdfAbstractData_Cpp
    LIBRARIES
        tf
        vt
        sdf
    CPPFILES
        testenv/testSdfAbstractData.cpp
) 

pxr_build_test(testSdfAttributeBlocking_Cpp
    LIBRARIES
        sdf
    CPPFILES
        testenv/testSdfAttributeBlocking.cpp
)

pxr_build_test(testSdfHardToReach
    LIBRARIES
        sdf
    CPPFILES
        testenv/testSdfHardToReach.cpp
)

pxr_build_test(testSdfIntegerCoding
    LIBRARIES
        sdf
    CPPFILES
        testenv/testSdfIntegerCoding.cpp
)

pxr_build_test(testSdfLayerHints
    LIBRARIES
        sdf
    CPPFILES
        testenv/testSdfLayerHints.cpp
)

pxr_build_test(testSdfMetaDataPlugInfo
    LIBRARIES
        sdf
    CPPFILES
        testenv/testSdfMetaDataPlugInfo.cpp
)

pxr_build_test(testSdfPathExpression_Cpp
    LIBRARIES
        sdf
    CPPFILES
        testenv/testSdfPathExpression.cpp
)

pxr_build_test(testSdfPathParser
    LIBRARIES
        sdf
    CPPFILES
        testenv/testSdfPathParser.cpp
)

pxr_build_test(testSdfPathTable
    LIBRARIES
        sdf
    CPPFILES
        testenv/testSdfPathTable.cpp
)

pxr_build_test(testSdfPathThreading
    LIBRARIES
        sdf
        tf
    CPPFILES
        testenv/testSdfPathThreading.cpp
)

pxr_build_test(testSdfPredicateExpression_Cpp
    LIBRARIES
        sdf
        tf
    CPPFILES
        testenv/testSdfPredicateExpression.cpp
)

pxr_build_test(testSdfUsdaFileFormatParsing
    LIBRARIES
        sdf
        tf
    CPPFILES
        testenv/testSdfUsdaFileFormatParsing.cpp
)

pxr_build_test(testSdfZipFile_CPP
    LIBRARIES
        ar
        arch
        tf
        sdf
    CPPFILES
        testenv/testSdfZipFile.cpp
)

pxr_build_test(testSdfUsdzResolver
    LIBRARIES
        ar
        arch
        tf
        sdf
    CPPFILES
        testenv/testSdfUsdzResolver.cpp
)

pxr_install_test_dir(
    SRC testenv/testSdfBatchNamespaceEdit.testenv
    DEST testSdfBatchNamespaceEdit/testSdfBatchNamespaceEdit.testenv
)

pxr_install_test_dir(
    SRC testenv/testSdfCustomLayerData.testenv
    DEST testSdfCustomLayerData/testSdfCustomLayerData.testenv
)

pxr_install_test_dir(
    SRC testenv/testSdfLayerHints.testenv
    DEST testSdfLayerHints/testSdfLayerHints.testenv
)

pxr_install_test_dir(
    SRC testenv/testSdfLayerMuting.testenv
    DEST testSdfLayerMuting/testSdfLayerMuting.testenv
)

pxr_install_test_dir(
    SRC testenv/testSdfMetaDataPlugInfo.testenv
    DEST testSdfMetaDataPlugInfo/testSdfMetaDataPlugInfo.testenv
)

pxr_install_test_dir(
    SRC testenv/testSdfParsing.testenv
    DEST testSdfParsing/testSdfParsing.testenv
)

pxr_install_test_dir(
    SRC testenv/testSdfTargetFileFormat.testenv
    DEST testSdfTargetFileFormat
)

pxr_install_test_dir(
    SRC testenv/testSdfZipFile.testenv
    DEST testSdfZipFile
)

pxr_install_test_dir(
    SRC testenv/testSdfZipFile.testenv
    DEST testSdfZipFile_CPP
)

pxr_install_test_dir(
    SRC testenv/testSdfUsdzResolver
    DEST testSdfUsdzResolver
)

pxr_register_test(testSdfAbstractData_Cpp
    COMMAND "${CMAKE_INSTALL_PREFIX}/tests/testSdfAbstractData_Cpp"
)

pxr_register_test(testSdfAssetPath
    PYTHON
    COMMAND "${CMAKE_INSTALL_PREFIX}/tests/testSdfAssetPath"
)

pxr_register_test(testSdfAttribute
    PYTHON
    COMMAND "${CMAKE_INSTALL_PREFIX}/tests/testSdfAttribute"
)

pxr_register_test(testSdfAttributeBlocking
    PYTHON
    COMMAND "${CMAKE_INSTALL_PREFIX}/tests/testSdfAttributeBlocking"
)

pxr_register_test(testSdfAttributeBlocking_Cpp
    COMMAND "${CMAKE_INSTALL_PREFIX}/tests/testSdfAttributeBlocking_Cpp"
)

pxr_register_test(testSdfBatchNamespaceEdit
    PYTHON
    COMMAND "${CMAKE_INSTALL_PREFIX}/tests/testSdfBatchNamespaceEdit"
)

pxr_register_test(testSdfCopyUtils
    PYTHON
    COMMAND "${CMAKE_INSTALL_PREFIX}/tests/testSdfCopyUtils"
)

pxr_register_test(testSdfCustomLayerData
    PYTHON
    COMMAND "${CMAKE_INSTALL_PREFIX}/tests/testSdfCustomLayerData"
)

pxr_register_test(testSdfDetachedLayer
    PYTHON
    COMMAND "${CMAKE_INSTALL_PREFIX}/tests/testSdfDetachedLayer"
)

pxr_register_test(testSdfDetachedLayerEnvVar1
    PYTHON
    ENV
        SDF_LAYER_INCLUDE_IN_MEMORY=*
        SDF_LAYER_EXCLUDE_IN_MEMORY=foo,bar
    COMMAND "${CMAKE_INSTALL_PREFIX}/tests/testSdfDetachedLayer"
)

pxr_register_test(testSdfDetachedLayerEnvVar2
    PYTHON
    ENV
        SDF_LAYER_INCLUDE_IN_MEMORY=foo,bar
        SDF_LAYER_EXCLUDE_IN_MEMORY=baz,garply
    COMMAND "${CMAKE_INSTALL_PREFIX}/tests/testSdfDetachedLayer"
)

pxr_register_test(testSdfHardToReach
    COMMAND "${CMAKE_INSTALL_PREFIX}/tests/testSdfHardToReach"
)

pxr_register_test(testSdfFileFormat
    PYTHON
    COMMAND "${CMAKE_INSTALL_PREFIX}/tests/testSdfFileFormat"
)

pxr_register_test(testSdfFileFormatCapabilities
    PYTHON
    COMMAND "${CMAKE_INSTALL_PREFIX}/tests/testSdfFileFormatCapabilities"
)

pxr_register_test(testSdfIntegerCoding
    COMMAND "${CMAKE_INSTALL_PREFIX}/tests/testSdfIntegerCoding"
    EXPECTED_RETURN_CODE 0
)

pxr_register_test(testSdfLayer
    PYTHON
    COMMAND "${CMAKE_INSTALL_PREFIX}/tests/testSdfLayer"
)

pxr_register_test(testSdfLayerHints
    COMMAND "${CMAKE_INSTALL_PREFIX}/tests/testSdfLayerHints"
)

pxr_register_test(testSdfLayerMuting
    PYTHON
    COMMAND "${CMAKE_INSTALL_PREFIX}/tests/testSdfLayerMuting"
)

pxr_register_test(testSdfLegacyFileFormat
    PYTHON
    ENV
        SDF_FILE_FORMAT_LEGACY_IMPORT=allow
    COMMAND "${CMAKE_INSTALL_PREFIX}/tests/testSdfLegacyFileFormat"
)

pxr_register_test(testSdfLegacyFileFormat_Error
    PYTHON
    ENV
        SDF_FILE_FORMAT_LEGACY_IMPORT=error
    COMMAND "${CMAKE_INSTALL_PREFIX}/tests/testSdfLegacyFileFormat"
)

pxr_register_test(testSdfListOp
    PYTHON
    COMMAND "${CMAKE_INSTALL_PREFIX}/tests/testSdfListOp"
)

pxr_register_test(testSdfMetaDataPlugInfo
    COMMAND "${CMAKE_INSTALL_PREFIX}/tests/testSdfMetaDataPlugInfo"
)

pxr_register_test(testSdfPathExpression
    PYTHON
    COMMAND "${CMAKE_INSTALL_PREFIX}/tests/testSdfPathExpression"
)

pxr_register_test(testSdfPathExpression_Cpp
    COMMAND "${CMAKE_INSTALL_PREFIX}/tests/testSdfPathExpression_Cpp"
)

pxr_register_test(testSdfPathExpressionArray
    PYTHON
    COMMAND "${CMAKE_INSTALL_PREFIX}/tests/testSdfPathExpressionArray"
)

pxr_register_test(testSdfParsing
    PYTHON
    COMMAND "${CMAKE_INSTALL_PREFIX}/tests/testSdfParsing"
)

pxr_register_test(testSdfPath
    PYTHON
    COMMAND "${CMAKE_INSTALL_PREFIX}/tests/testSdfPath"
)

pxr_register_test(testSdfPath2
    PYTHON
    COMMAND "${CMAKE_INSTALL_PREFIX}/tests/testSdfPath2"
)
pxr_register_test(testSdfPath2Construct
    PYTHON
    COMMAND "${CMAKE_INSTALL_PREFIX}/tests/testSdfPath2Construct"
)

pxr_register_test(testSdfPathParser
    COMMAND "${CMAKE_INSTALL_PREFIX}/tests/testSdfPathParser"
)

pxr_register_test(testSdfPathTable
    COMMAND "${CMAKE_INSTALL_PREFIX}/tests/testSdfPathTable"
)

pxr_register_test(testSdfPathThreading
    COMMAND "${CMAKE_INSTALL_PREFIX}/tests/testSdfPathThreading"
)

pxr_register_test(testSdfPredicateExpression_Cpp
    COMMAND "${CMAKE_INSTALL_PREFIX}/tests/testSdfPredicateExpression_Cpp"
)

pxr_register_test(testSdfUsdaFileFormatParsing
   COMMAND "${CMAKE_INSTALL_PREFIX}/tests/testSdfUsdaFileFormatParsing"
)

pxr_register_test(testSdfPayload
    PYTHON
    COMMAND "${CMAKE_INSTALL_PREFIX}/tests/testSdfPayload"
)
pxr_register_test(testSdfPrim
    PYTHON
    COMMAND "${CMAKE_INSTALL_PREFIX}/tests/testSdfPrim"
)
pxr_register_test(testSdfReference
    PYTHON
    COMMAND "${CMAKE_INSTALL_PREFIX}/tests/testSdfReference"
)
pxr_register_test(testSdfRelocates
    PYTHON
    COMMAND "${CMAKE_INSTALL_PREFIX}/tests/testSdfRelocates"
)
pxr_register_test(testSdfSpecHash
    PYTHON
    COMMAND "${CMAKE_INSTALL_PREFIX}/tests/testSdfSpecHash"
)
pxr_register_test(testSdfTargetFileFormat
    PYTHON
    COMMAND "${CMAKE_INSTALL_PREFIX}/tests/testSdfTargetFileFormat"
)
pxr_register_test(testSdfTimeCode
    PYTHON
    COMMAND "${CMAKE_INSTALL_PREFIX}/tests/testSdfTimeCode"
)
pxr_register_test(testSdfTypes
    PYTHON
    COMMAND "${CMAKE_INSTALL_PREFIX}/tests/testSdfTypes"
)
pxr_register_test(testSdfZipFile
    PYTHON
    COMMAND "${CMAKE_INSTALL_PREFIX}/tests/testSdfZipFile"
    EXPECTED_RETURN_CODE 0
)
pxr_register_test(testSdfZipFile_CPP
    COMMAND "${CMAKE_INSTALL_PREFIX}/tests/testSdfZipFile_CPP"
    EXPECTED_RETURN_CODE 0
)
pxr_register_test(testSdfUsdzResolver
    COMMAND "${CMAKE_INSTALL_PREFIX}/tests/testSdfUsdzResolver"
    EXPECTED_RETURN_CODE 0
)
pxr_register_test(testSdfVariableExpression
    PYTHON
    COMMAND "${CMAKE_INSTALL_PREFIX}/tests/testSdfVariableExpression"
)
pxr_register_test(testSdfVariants
    PYTHON
    COMMAND "${CMAKE_INSTALL_PREFIX}/tests/testSdfVariants"
)
