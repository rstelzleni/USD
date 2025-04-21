set(PXR_PREFIX pxr/exec)
set(PXR_PACKAGE exec)

pxr_library(exec
    LIBRARIES
        ef
        esf
        tf
        trace
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
        types.h

    PUBLIC_CLASSES
        builtinComputations
        cacheView
        computationBuilders
        requestImpl
        system
        systemChangeManager
        systemDiagnostics
        typeRegistry
        valueKey

    PRIVATE_HEADERS
        compilationState.h
        inputKey.h

    PRIVATE_CLASSES
        builtinStageComputations
        callbackNode
        compilationTask
        compiledOutputCache
        compiler
        compilerTaskSync
        computationDefinition
        definitionRegistry
        inputResolver
        inputResolvingCompilationTask
        leafCompilationTask
        outputKey
        outputProvidingCompilationTask
        program
        uncompilationRuleSet
        uncompilationTable

    RESOURCE_FILES
        plugInfo.json
)

pxr_build_test(testExecComputationRegistration
    LIBRARIES
        ef
        esf
        exec
        execUsd
        plug
        sdf
        tf
        usd
    CPPFILES
        testenv/testExecComputationRegistration.cpp
)

pxr_install_test_dir(
    SRC testenv/testExecComputationRegistration
    DEST testExecComputationRegistration
)

pxr_register_test(testExecComputationRegistration
    COMMAND "${CMAKE_INSTALL_PREFIX}/tests/testExecComputationRegistration"
    EXPECTED_RETURN_CODE 0
)

pxr_build_test(testExecInputResolver
    LIBRARIES
        ef
        esf
        exec
        execUsd
        sdf
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
