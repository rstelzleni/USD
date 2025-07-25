#
# Copyright 2016 Pixar
#
# Licensed under the terms set forth in the LICENSE.txt file available at
# https://openusd.org/license.
#
option(PXR_STRICT_BUILD_MODE "Turn on additional warnings. Enforce all warnings as errors." OFF)
option(PXR_VALIDATE_GENERATED_CODE "Validate script generated code" OFF)
option(PXR_HEADLESS_TEST_MODE "Disallow GUI based tests, useful for running under headless CI systems." OFF)
option(PXR_BUILD_TESTS "Build tests" ON)
option(PXR_BUILD_EXAMPLES "Build examples" ON)
option(PXR_BUILD_TUTORIALS "Build tutorials" ON)
option(PXR_BUILD_USD_TOOLS "Build commandline tools" ON)
option(PXR_BUILD_IMAGING "Build imaging components" ON)
option(PXR_BUILD_EMBREE_PLUGIN "Build embree imaging plugin" OFF)
option(PXR_BUILD_OPENIMAGEIO_PLUGIN "Build OpenImageIO plugin" OFF)
if(APPLE)
    option(PXR_BUILD_IMAGEIO_PLUGIN "Build the ImageIO.framework plugin for Apple platforms" ON)
endif()
option(PXR_BUILD_OPENCOLORIO_PLUGIN "Build OpenColorIO plugin" OFF)
option(PXR_BUILD_USD_IMAGING "Build USD imaging components" ON)
option(PXR_BUILD_USD_VALIDATION "Build USD validation library and core USD validators" ON)
option(PXR_BUILD_EXEC "Build the Exec libraries" ON)
option(PXR_BUILD_USDVIEW "Build usdview" ON)
option(PXR_BUILD_ALEMBIC_PLUGIN "Build the Alembic plugin for USD" OFF)
option(PXR_BUILD_DRACO_PLUGIN "Build the Draco plugin for USD" OFF)
option(PXR_BUILD_PRMAN_PLUGIN "Build the PRMan imaging plugin" OFF)
option(PXR_ENABLE_MATERIALX_SUPPORT "Enable MaterialX support" OFF)
option(PXR_BUILD_DOCUMENTATION "Generate doxygen documentation" OFF)
option(PXR_BUILD_PYTHON_DOCUMENTATION "Generate Python documentation" OFF)
option(PXR_BUILD_HTML_DOCUMENTATION "Generate HTML documentation if PXR_BUILD_DOCUMENTATION is ON" ON)
option(PXR_ENABLE_PYTHON_SUPPORT "Enable Python based components for USD" ON)
option(PXR_USE_DEBUG_PYTHON "Build with debug python" OFF)
option(PXR_ENABLE_HDF5_SUPPORT "Enable HDF5 backend in the Alembic plugin for USD" OFF)
option(PXR_ENABLE_OSL_SUPPORT "Enable OSL (OpenShadingLanguage) based components" OFF)
option(PXR_ENABLE_PTEX_SUPPORT "Enable Ptex support" OFF)
option(PXR_ENABLE_OPENVDB_SUPPORT "Enable OpenVDB support" OFF)
option(PXR_BUILD_MAYAPY_TESTS "Build mayapy spline tests" OFF)
option(PXR_BUILD_ANIMX_TESTS "Build AnimX spline tests" OFF)
option(PXR_ENABLE_NAMESPACES "Enable C++ namespaces." ON)
option(PXR_PREFER_SAFETY_OVER_SPEED
       "Enable certain checks designed to avoid crashes or out-of-bounds memory reads with malformed input files.  These checks may negatively impact performance."
        ON)

if(APPLE)
    # Cross Compilation detection as defined in CMake docs
    # Required to be handled here so it can configure options later on
    # https://cmake.org/cmake/help/latest/manual/cmake-toolchains.7.html#cross-compiling-for-ios-tvos-visionos-or-watchos
    # Note: All these SDKs may not be supported by OpenUSD, but are all listed here for future proofing
    set(PXR_APPLE_EMBEDDED OFF)
    if (CMAKE_SYSTEM_NAME MATCHES "iOS"
            OR CMAKE_SYSTEM_NAME MATCHES "tvOS"
            OR CMAKE_SYSTEM_NAME MATCHES "visionOS"
            OR CMAKE_SYSTEM_NAME MATCHES "watchOS")
        set(PXR_APPLE_EMBEDDED ON)
        if(${PXR_BUILD_USD_TOOLS})
            MESSAGE(STATUS "Setting PXR_BUILD_USD_TOOLS=OFF because they are not supported on Apple embedded platforms")
            set(PXR_BUILD_USD_TOOLS OFF)
        endif()
        if(${PXR_BUILD_OPENCOLORIO_PLUGIN})
            MESSAGE(STATUS "Setting PXR_BUILD_OPENCOLORIO_PLUGIN=OFF because it is not supported on Apple embedded platforms")
            set(PXR_BUILD_OPENCOLORIO_PLUGIN OFF)
        endif()
        if(${PXR_BUILD_OPENIMAGEIO_PLUGIN})
            MESSAGE(STATUS "Setting PXR_BUILD_OPENIMAGEIO_PLUGIN=OFF because it is not supported on Apple embedded platforms")
            set(PXR_BUILD_OPENIMAGEIO_PLUGIN OFF)
        endif()
        if(${PXR_ENABLE_OPENVDB_SUPPORT})
            MESSAGE(STATUS "Setting PXR_ENABLE_OPENVDB_SUPPORT=OFF because it is not supported on Apple embedded platforms")
            set(PXR_ENABLE_OPENVDB_SUPPORT OFF)
        endif()
    endif ()
endif()


# Determine GFX api
# Metal only valid on Apple platforms
set(pxr_enable_metal "OFF")
if(APPLE)
    set(pxr_enable_metal "ON")
endif()
option(PXR_ENABLE_METAL_SUPPORT "Enable Metal based components" "${pxr_enable_metal}")
option(PXR_ENABLE_VULKAN_SUPPORT "Enable Vulkan based components" OFF)
option(PXR_ENABLE_GL_SUPPORT "Enable OpenGL based components" ON)

# Precompiled headers are a win on Windows, not on gcc.
set(pxr_enable_pch "OFF")
if(MSVC)
    set(pxr_enable_pch "ON")
endif()
option(PXR_ENABLE_PRECOMPILED_HEADERS "Enable precompiled headers." "${pxr_enable_pch}")
set(PXR_PRECOMPILED_HEADER_NAME "pch.h"
    CACHE
    STRING
    "Default name of precompiled header files"
)

set(PXR_INSTALL_LOCATION ""
    CACHE
    STRING
    "Intended final location for plugin resource files."
)

set(PXR_OVERRIDE_PLUGINPATH_NAME ""
    CACHE
    STRING
    "Name of the environment variable that will be used to get plugin paths."
)

set(PXR_TEST_RUN_TEMP_DIR_PREFIX ""
    CACHE
    STRING
    "Prefix for test run temporary directory names. \
    Setting this option to \"foo-\" will create directories like \
    \"<temp dir>/foo-<test dir>\"."
)

set(PXR_ALL_LIBS ""
    CACHE
    INTERNAL
    "Aggregation of all built libraries."
)
set(PXR_STATIC_LIBS ""
    CACHE
    INTERNAL
    "Aggregation of all built explicitly static libraries."
)
set(PXR_CORE_LIBS ""
    CACHE
    INTERNAL
    "Aggregation of all built core libraries."
)
set(PXR_OBJECT_LIBS ""
    CACHE
    INTERNAL
    "Aggregation of all core libraries built as OBJECT libraries."
)

string(CONCAT helpstr
    "Prefix for built library filenames. If unspecified, defaults "
    "to 'libusd_' on Linux/macOS and 'usd_' on Windows, or '' for "
    "monolithic builds."
)
set(PXR_LIB_PREFIX ""
    CACHE
    STRING
    "${helpstr}"
)

option(BUILD_SHARED_LIBS "Build shared libraries." ON)
option(PXR_BUILD_MONOLITHIC "Build a monolithic library." OFF)
set(PXR_MONOLITHIC_IMPORT ""
    CACHE
    STRING
    "Path to cmake file that imports a usd_ms target"
)

set(PXR_EXTRA_PLUGINS ""
    CACHE
    INTERNAL
    "Aggregation of extra plugin directories containing a plugInfo.json.")

# Resolve options that depend on one another so that subsequent .cmake scripts
# all have the final value for these options.
if (${PXR_BUILD_USD_IMAGING} AND NOT ${PXR_BUILD_IMAGING})
    message(STATUS
        "Setting PXR_BUILD_USD_IMAGING=OFF because PXR_BUILD_IMAGING=OFF")
    set(PXR_BUILD_USD_IMAGING "OFF" CACHE BOOL "" FORCE)
endif()

if (${PXR_ENABLE_METAL_SUPPORT})
    if (NOT APPLE)
        message(STATUS
            "Setting PXR_ENABLE_METAL_SUPPORT=OFF because Metal is only supported on macOS")
        set(PXR_ENABLE_METAL_SUPPORT "OFF" CACHE BOOL "" FORCE)
    endif()
endif()

if (${PXR_ENABLE_GL_SUPPORT} OR ${PXR_ENABLE_METAL_SUPPORT} OR ${PXR_ENABLE_VULKAN_SUPPORT})
    set(PXR_BUILD_GPU_SUPPORT "ON")
else()
    set(PXR_BUILD_GPU_SUPPORT "OFF")
endif()

if (${PXR_BUILD_USDVIEW})
    if (NOT ${PXR_BUILD_USD_IMAGING})
        message(STATUS
            "Setting PXR_BUILD_USDVIEW=OFF because "
            "PXR_BUILD_USD_IMAGING=OFF")
        set(PXR_BUILD_USDVIEW "OFF" CACHE BOOL "" FORCE)
    elseif (NOT ${PXR_ENABLE_PYTHON_SUPPORT})
        message(STATUS
            "Setting PXR_BUILD_USDVIEW=OFF because "
            "PXR_ENABLE_PYTHON_SUPPORT=OFF")
        set(PXR_BUILD_USDVIEW "OFF" CACHE BOOL "" FORCE)
    elseif (NOT ${PXR_BUILD_GPU_SUPPORT})
        message(STATUS
            "Setting PXR_BUILD_USDVIEW=OFF because "
            "PXR_BUILD_GPU_SUPPORT=OFF")
        set(PXR_BUILD_USDVIEW "OFF" CACHE BOOL "" FORCE)
    endif()
endif()

if (${PXR_BUILD_EMBREE_PLUGIN})
    if (NOT ${PXR_BUILD_IMAGING})
        message(STATUS
            "Setting PXR_BUILD_EMBREE_PLUGIN=OFF because PXR_BUILD_IMAGING=OFF")
        set(PXR_BUILD_EMBREE_PLUGIN "OFF" CACHE BOOL "" FORCE)
    elseif (NOT ${PXR_BUILD_GPU_SUPPORT})
        message(STATUS
            "Setting PXR_BUILD_EMBREE_PLUGIN=OFF because "
            "PXR_BUILD_GPU_SUPPORT=OFF")
        set(PXR_BUILD_EMBREE_PLUGIN "OFF" CACHE BOOL "" FORCE)
    endif()
endif()

if (${PXR_BUILD_PRMAN_PLUGIN})
    if (NOT ${PXR_BUILD_IMAGING})
        message(STATUS
            "Setting PXR_BUILD_PRMAN_PLUGIN=OFF because PXR_BUILD_IMAGING=OFF")
        set(PXR_BUILD_PRMAN_PLUGIN "OFF" CACHE BOOL "" FORCE)
    endif()
endif()

# Error out if user is building monolithic library on windows with draco plugin
# enabled. This currently results in missing symbols.
if (${PXR_BUILD_DRACO_PLUGIN} AND ${PXR_BUILD_MONOLITHIC} AND WIN32)
    message(FATAL_ERROR 
        "Draco plugin can not be enabled for monolithic builds on Windows")
endif()

# Make sure PXR_BUILD_DOCUMENTATION and PXR_ENABLE_PYTHON_SUPPORT are enabled 
# if PXR_BUILD_PYTHON_DOCUMENTATION is enabled
if (${PXR_BUILD_PYTHON_DOCUMENTATION})
    if (NOT ${PXR_BUILD_DOCUMENTATION})
        message(STATUS
            "Setting PXR_BUILD_PYTHON_DOCUMENTATION=OFF because "
            "PXR_BUILD_DOCUMENTATION=OFF")
        set(PXR_BUILD_PYTHON_DOCUMENTATION "OFF" CACHE BOOL "" FORCE)
    elseif (NOT ${PXR_ENABLE_PYTHON_SUPPORT})
        message(STATUS
            "Setting PXR_BUILD_PYTHON_DOCUMENTATION=OFF because "
            "PXR_ENABLE_PYTHON_SUPPORT=OFF")
        set(PXR_BUILD_PYTHON_DOCUMENTATION "OFF" CACHE BOOL "" FORCE)
    endif()
endif()
