set(PXR_PREFIX pxr/usdImaging)
set(PXR_PACKAGE hydraPassthrough)

pxr_library(hydraPassthrough
    LIBRARIES
        gf
        tf
        hd
        hdsi
        sdf
        usd
        usdImaging

    PUBLIC_CLASSES
        mesh
        renderDelegate
        renderManager
        rendererPlugin
        renderPass
        test

    PUBLIC_HEADERS
        api.h

    PYMODULE_CPPFILES
        module.cpp
        wrapRenderManager.cpp
        wrapTest.cpp

    PYMODULE_FILES
        __init__.py

    RESOURCE_FILES
        plugInfo.json
)

