set(PXR_PREFIX pxr/exec)
set(PXR_PACKAGE esfUsd)

pxr_library(esfUsd
    LIBRARIES
        arch
        esf
        tf
        sdf
        usd
        
    PUBLIC_HEADERS
        api.h

    PUBLIC_CLASSES
        sceneAdapter

    PRIVATE_CLASSES
        attribute
        object
        prim
        property
        stage

    RESOURCE_FILES
        plugInfo.json
)
