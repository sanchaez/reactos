
spec2def(bootvid.dll bootvid.spec ADD_IMPORTLIB)

if((ARCH STREQUAL "i386") OR (ARCH STREQUAL "amd64"))
    list(APPEND SOURCE
        i386/bootvid.c
        i386/bootdata.c
        i386/vga.c)
elseif(ARCH STREQUAL "arm")
    list(APPEND SOURCE
        arm/bootvid.c)
endif()

list(APPEND SOURCE
    fontdata.c
    precomp.h)

add_library(bootvid MODULE
    ${SOURCE}
    bootvid.rc
    ${CMAKE_CURRENT_BINARY_DIR}/bootvid.def)

set_module_type(bootvid kerneldll ENTRYPOINT 0)
add_importlibs(bootvid ntoskrnl hal)
add_pch(bootvid precomp.h SOURCE)
add_dependencies(bootvid psdk)
add_cd_file(TARGET bootvid DESTINATION reactos/system32 NO_CAB FOR all)
