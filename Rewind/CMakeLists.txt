cmake_minimum_required(VERSION 3.16)
project(PauseItRewind CXX)

set(HL2SDK_PATH ${PROJECT_SOURCE_DIR}/hl2sdk)
set(METAMOD_SOURCE ${PROJECT_SOURCE_DIR}/metamod-source)
set(SOURCEMOD ${PROJECT_SOURCE_DIR}/sourcemod)

set(CMAKE_CXX_STANDARD 20)

add_compile_options(-m32)
add_link_options(-m32)

# We are dlopen'd and dlclose'd, so this is nice-to-have.
add_compile_options(-fno-gnu-unique)

add_subdirectory(JMP)
set(SUBHOOK_TESTS OFF)
set(SUBHOOK_STATIC ON)
add_subdirectory(subhook)

add_library(PauseItRewind SHARED
        src/PauseItRewind.cpp
        ${SOURCEMOD}/public/smsdk_ext.cpp
)

target_compile_definitions(PauseItRewind PRIVATE
        SOURCE_ENGINE=11
        stricmp=strcasecmp
        _stricmp=strcasecmp
        _snprintf=snprintf
        _vsnprintf=vsnprintf
        GNUC
        _LINUX
        POSIX
        NO_MALLOC_OVERRIDE
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
        ${HL2SDK_PATH}/public/mathlib
        ${HL2SDK_PATH}/public/vstdlib
        ${HL2SDK_PATH}/public/tier0
        ${HL2SDK_PATH}/public/tier1
        ${HL2SDK_PATH}/public/game/server
        ${METAMOD_SOURCE}/core
        ${METAMOD_SOURCE}/core/sourcehook

        ${SOURCEMOD}
        ${SOURCEMOD}/public
        ${SOURCEMOD}/public/extensions
        ${SOURCEMOD}/sourcepawn/include
        ${SOURCEMOD}/public/amtl/amtl
        ${SOURCEMOD}/public/amtl
        ${SOURCEMOD}/bridge/include/

        JMP/src

        ${PROJECT_SOURCE_DIR}/src
)

set_target_properties(PauseItRewind PROPERTIES PREFIX "")
target_link_libraries(PauseItRewind PRIVATE JMP subhook::subhook)
