set(DIRS
    vdf
    ef
    esf
    esfUsd
    exec
    execUsd
)

foreach(d ${DIRS})
    add_subdirectory(${d})
endforeach()