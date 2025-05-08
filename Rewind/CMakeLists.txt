cmake_minimum_required(VERSION 3.16)
project(PauseItRewind CXX)

set(HL2SDK_PATH ${PROJECT_SOURCE_DIR}/hl2sdk)
set(METAMOD_SOURCE ${PROJECT_SOURCE_DIR}/metamod-source)

set(CMAKE_CXX_STANDARD 20)

add_compile_options(-m32)
add_link_options(-m32)

# mathlib.h uses register and just spams a ton of warnings, so let's just ignore them, no one is actually going to use
# register anyways.
add_compile_options(-Wno-register)

add_compile_options(-fno-gnu-unique)

add_subdirectory(JMP)
set(SUBHOOK_TESTS OFF)
set(SUBHOOK_STATIC ON)
add_subdirectory(subhook)

add_library(PauseItRewind SHARED
        src/PauseItRewind.cpp
)

# FIXME: Rebasing/merging alliedmodders/hl2sdk into drunderscore/hl2sdk got us the VScript changes, but it also got us
#        some changes to utlvector.h. These changes do not compile on Linux without -fpermissive.
#        On it's own it's not that problematic, but it means that ALL of my code will also be compiled with
#        -fpermissive, which isn't desirable at all.
target_compile_options(PauseItRewind PRIVATE -fpermissive)

target_compile_definitions(PauseItRewind PRIVATE
        SOURCE_ENGINE=11
        stricmp=strcasecmp
        _stricmp=strcasecmp
        _snprintf=snprintf
        _vsnprintf=vsnprintf
        HAVE_STDINT_H
        GNUC
        _LINUX
        POSIX
        # These are all from sourcemod/AMBuildScript
        SE_EPISODEONE=1
        SE_ORANGEBOX=3
        SE_CSS=6
        SE_HL2DM=7
        SE_DODS=8
        SE_SDK2013=9
        SE_LEFT4DEAD=12
        SE_NUCLEARDAWN=13
        SE_LEFT4DEAD2=15
        SE_DARKMESSIAH=2
        SE_ALIENSWARM=16
        SE_BLOODYGOODTIME=4
        SE_EYE=5
        SE_CSGO=21
        SE_PORTAL2=17
        SE_BLADE=18
        SE_INSURGENCY=19
        SE_CONTAGION=14
        SE_DOI=20
        SE_MOCK=999
)
target_include_directories(PauseItRewind PRIVATE SYSTEM
        ${PROJECT_SOURCE_DIR}
        ${HL2SDK_PATH}/common
        ${HL2SDK_PATH}/public
        ${HL2SDK_PATH}/public/engine
        ${HL2SDK_PATH}/public/mathlib
        ${HL2SDK_PATH}/public/vstdlib
        ${HL2SDK_PATH}/public/tier0
        ${HL2SDK_PATH}/public/tier1
        ${HL2SDK_PATH}/public/game/server
        ${METAMOD_SOURCE}/core
        ${METAMOD_SOURCE}/core/sourcehook

        JMP/src
)

set_target_properties(PauseItRewind PROPERTIES PREFIX "")
target_link_directories(PauseItRewind PRIVATE ${HL2SDK_PATH}/lib/linux)
target_link_libraries(PauseItRewind PRIVATE -l:tier1_i486.a -l:libvstdlib_srv.so -l:libtier0_srv.so JMP subhook::subhook -l:mathlib_i486.a)
