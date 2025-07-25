set(PXR_PREFIX pxr/exec)
set(PXR_PACKAGE exec)

pxr_library(exec
    LIBRARIES
        ef
        esf
        tf
        trace
        ts
        sdf
        usd
        vdf
        vt
        ${TBB_tbb_LIBRARY}

    INCLUDE_DIRS
        ${TBB_INCLUDE_DIRS}
        
    PUBLIC_HEADERS
        api.h
        providerResolution.h
        registerSchema.h
        request.h
        types.h
        valueExtractorFunction.h

    PUBLIC_CLASSES
        builtinComputations
        cacheView
        computationBuilders
        requestImpl
        system
        systemChangeProcessor
        systemDiagnostics
        typeRegistry
        valueKey

    PRIVATE_HEADERS
        authoredValueInvalidationResult.h
        compilationState.h
        disconnectedInputsInvalidationResult.h
        inputKey.h
        parallelForRange.h
        timeChangeInvalidationResult.h

    PRIVATE_CLASSES
        attributeInputNode
        builtinAttributeComputations
        builtinStageComputations
        callbackNode
        compilationTask
        compiledLeafNodeCache
        compiledOutputCache
        compiler
        compilerTaskSync
        computationDefinition
        definitionRegistry
        inputRecompilationTask
        inputResolver
        inputResolvingCompilationTask
        leafCompilationTask
        nodeRecompilationInfo
        nodeRecompilationInfoTable
        outputKey
        outputProvidingCompilationTask
        pluginComputationDefinition
        program
        registrationBarrier
        requestTracker
        runtime
        uncompilationRuleSet
        uncompilationTable
        uncompilationTarget
        uncompiler
        valueExtractor

    RESOURCE_FILES
        plugInfo.json
)

#
# Tests
#

# Build test plugins

pxr_build_test_shared_lib(TestExecPluginComputation
    INSTALL_PREFIX ExecPlugins
    LIBRARIES
        tf
        exec
        
    CPPFILES
        testenv/TestExecPluginComputation.cpp
)

pxr_build_test_shared_lib(TestExecConflictingPluginRegistration1
    INSTALL_PREFIX ExecPlugins
    LIBRARIES
        tf
        exec
        
    CPPFILES
        testenv/TestExecConflictingPluginRegistration1.cpp
)

pxr_build_test_shared_lib(TestExecConflictingPluginRegistration2
    INSTALL_PREFIX ExecPlugins
    LIBRARIES
        tf
        exec
        
    CPPFILES
        testenv/TestExecConflictingPluginRegistration2.cpp
)

# Build tests

pxr_build_test(testExecComputationRegistration
    LIBRARIES
        ef
        esf
        esfUsd
        exec
        plug
        sdf
        tf
        usd
    CPPFILES
        testenv/testExecComputationRegistration.cpp
)

# Tests that rely on plugins aren't currently supported in static builds.
if (BUILD_SHARED_LIBS)
    pxr_install_test_dir(
        SRC testenv/testExecComputationRegistration
        DEST testExecComputationRegistration
    )

    pxr_register_test(testExecComputationRegistration
        COMMAND "${CMAKE_INSTALL_PREFIX}/tests/testExecComputationRegistration"
        EXPECTED_RETURN_CODE 0
    )
endif()

pxr_build_test(testExecInputResolver
    LIBRARIES
        ef
        esf
        esfUsd
        exec
        plug
        sdf
        tf
        usd
    CPPFILES
        testenv/testExecInputResolver.cpp
)

pxr_install_test_dir(
    SRC testenv/testExecInputResolver
    DEST testExecInputResolver
)

pxr_register_test(testExecInputResolver
    COMMAND "${CMAKE_INSTALL_PREFIX}/tests/testExecInputResolver"
    EXPECTED_RETURN_CODE 0
)

pxr_build_test(testExecTypeRegistration
    LIBRARIES
        gf
        tf
        exec
    CPPFILES
        testenv/testExecTypeRegistration.cpp
)

pxr_register_test(testExecTypeRegistration
    COMMAND "${CMAKE_INSTALL_PREFIX}/tests/testExecTypeRegistration"
    EXPECTED_RETURN_CODE 0
)

pxr_build_test(testExecUncompilationTable
    LIBRARIES
        exec
        work
    CPPFILES
        testenv/testExecUncompilationTable.cpp
)

pxr_register_test(testExecUncompilationTable
    COMMAND "${CMAKE_INSTALL_PREFIX}/tests/testExecUncompilationTable"
    EXPECTED_RETURN_CODE 0
)
