set(DIRS
    usdrecord
    usdview
    testusdview
)

foreach(d ${DIRS})
    add_subdirectory(${d})
endforeach()

if (${PXR_ENABLE_MATERIALX_SUPPORT} AND ${PXR_BUILD_USD_BAKE_MTLX_TOOL})
    add_subdirectory(usdBakeMtlx)
endif()
