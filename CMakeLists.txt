cmake_minimum_required(VERSION 3.20)
project(PokemonStadiumRecomp C CXX)

set(CMAKE_C_STANDARD 17)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang" AND CMAKE_CXX_SIMULATE_ID STREQUAL "MSVC")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-everything /W4")
endif()
if (WIN32)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -D_DISABLE_CONSTEXPR_MUTEX_CONSTRUCTOR")
endif()
if (CMAKE_VERSION VERSION_GREATER_EQUAL "3.24.0")
    cmake_policy(SET CMP0135 NEW)
endif()

set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

# ---- ROM hash check ---------------------------------------------------------
set(EXPECTED_BASEROM_MD5 "ed1378bc12115f71209a77844965ba50")
if(EXISTS "${CMAKE_SOURCE_DIR}/baserom.z64")
    file(MD5 "${CMAKE_SOURCE_DIR}/baserom.z64" ACTUAL_BASEROM_MD5)
    if(NOT ACTUAL_BASEROM_MD5 STREQUAL EXPECTED_BASEROM_MD5)
        message(WARNING "baserom.z64 MD5 mismatch.\n  expected: ${EXPECTED_BASEROM_MD5}  (US v1.0)\n  actual:   ${ACTUAL_BASEROM_MD5}")
    else()
        message(STATUS "baserom.z64 MD5 OK (US v1.0)")
    endif()
endif()

# ---- RT64 (graphics renderer) ----------------------------------------------
set(RT64_STATIC TRUE)
set(RT64_SDL_WINDOW_VULKAN TRUE)
add_compile_definitions(HLSL_CPU)
add_subdirectory(${CMAKE_SOURCE_DIR}/lib/rt64 ${CMAKE_BINARY_DIR}/rt64)

# ---- N64ModernRuntime (junctioned at lib/N64ModernRuntime) -----------------
add_subdirectory(${CMAKE_SOURCE_DIR}/lib/N64ModernRuntime)

target_include_directories(rt64 PRIVATE ${CMAKE_BINARY_DIR}/rt64/src)

# ---- ares-bridge (oracle for divergence diff against Ares) -----------------
# The ares-bridge subdir is added transitively by librecomp's
# add_subdirectory of N64Recomp, so we don't need our own
# add_subdirectory here. Pass -DWITH_ARES_BRIDGE=ON to opt into the
# real Ares-backed implementation (the placeholder is always built).
# We just declare the include path + link dependency below.

# ---- RecompiledFuncs library (the recompiled game C) -----------------------
add_library(RecompiledFuncs STATIC)
target_compile_options(RecompiledFuncs PRIVATE
    -fno-strict-aliasing
    -Wno-unused-variable
    -Wno-implicit-function-declaration
)
target_include_directories(RecompiledFuncs PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/lib/N64ModernRuntime/ultramodern/include
    ${CMAKE_SOURCE_DIR}/lib/N64ModernRuntime/librecomp/include
    ${CMAKE_SOURCE_DIR}/lib/N64ModernRuntime/N64Recomp/include
)
file(GLOB FUNC_C_SOURCES CONFIGURE_DEPENDS ${CMAKE_SOURCE_DIR}/generated/*.c)
target_sources(RecompiledFuncs PRIVATE ${FUNC_C_SOURCES})

# ---- Runner executable ------------------------------------------------------
add_executable(PokemonStadiumRecomp
    src/main/main.cpp
    src/main/post_mortem.cpp
    src/main/rt64_render_context.cpp
    src/main/debug_server.cpp
    src/main/ares_worker.cpp
    src/main/register_overlays.cpp
    src/main/rsp_aspmain_hook.cpp
    src/main/transfer_pak.cpp
    extras.c
    # Generated lookup table (provides get_entrypoint_address etc).
    generated/lookup.cpp
    # Pokemon Stadium's own aspMain (recompiled from baserom @ 0x68020, 0xC60 bytes).
    ${CMAKE_SOURCE_DIR}/rsp/aspMain.cpp
    # GB Tower display/copy microcode from fragment 1 (selected by ucode boot signature).
    ${CMAKE_SOURCE_DIR}/rsp/gbTowerMain.cpp
    ${CMAKE_SOURCE_DIR}/rsp/gbTowerColorMain.cpp
    # njpgdspMain is the standard libultra JPEG ucode — borrow Zelda's recompiled output.
    ${CMAKE_SOURCE_DIR}/../Zelda64Recomp/rsp/njpgdspMain.cpp
)

target_include_directories(PokemonStadiumRecomp PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/lib/N64ModernRuntime/ultramodern/include
    ${CMAKE_SOURCE_DIR}/lib/N64ModernRuntime/librecomp/include
    ${CMAKE_SOURCE_DIR}/lib/N64ModernRuntime/N64Recomp/include
    ${CMAKE_SOURCE_DIR}/lib/concurrentqueue
    ${CMAKE_SOURCE_DIR}/lib/SlotMap
    ${CMAKE_SOURCE_DIR}/lib/rt64/src
    ${CMAKE_SOURCE_DIR}/lib/rt64/src/contrib
    ${CMAKE_SOURCE_DIR}/lib/rt64/src/contrib/hlslpp/include
    ${CMAKE_SOURCE_DIR}/lib/rt64/src/contrib/dxc/inc
    ${CMAKE_SOURCE_DIR}/lib/rt64/src/rhi
    ${CMAKE_SOURCE_DIR}/lib/rt64/src/render
)

target_compile_options(PokemonStadiumRecomp PRIVATE
    -fno-strict-aliasing
    -fms-extensions
    -march=nehalem  # Required by RT64's RSP simulator (SSSE3, SSE4.1, SSE4.2)
)

# Emit PDBs in Release for the runner + librecomp/ultramodern so the SEH
# crash filter's stack walk produces real symbol names. clang-cl needs
# /Z7 (embed CodeView in objects) on the compiler side and /DEBUG on the
# linker side; /OPT:REF + /OPT:NOICF keep dead-code stripping but stop
# IDENTICAL COMDAT folding from collapsing different functions to the
# same address (which would corrupt the stack-walk symbol mapping).
if (MSVC)
    target_compile_options(PokemonStadiumRecomp PRIVATE /Z7)
    target_compile_options(RecompiledFuncs       PRIVATE /Z7)
    target_link_options(PokemonStadiumRecomp PRIVATE /DEBUG /OPT:REF /OPT:NOICF)
    if (TARGET librecomp)
        target_compile_options(librecomp PRIVATE /Z7)
    endif()
    if (TARGET ultramodern)
        target_compile_options(ultramodern PRIVATE /Z7)
    endif()
endif()

# ---- SDL2 (fetched on Windows; standard system package on Linux/Mac) -------
if (WIN32)
    include(FetchContent)
    if (DEFINED ENV{SDL2_VERSION})
        set(SDL2_VERSION $ENV{SDL2_VERSION})
    else()
        set(SDL2_VERSION "2.30.3")
    endif()
    FetchContent_Declare(
        sdl2
        URL https://github.com/libsdl-org/SDL/releases/download/release-${SDL2_VERSION}/SDL2-devel-${SDL2_VERSION}-VC.zip
    )
    FetchContent_MakeAvailable(sdl2)
    target_include_directories(PokemonStadiumRecomp PRIVATE ${sdl2_SOURCE_DIR}/include)
    target_link_directories(PokemonStadiumRecomp PRIVATE ${sdl2_SOURCE_DIR}/lib/x64)

    add_custom_command(TARGET PokemonStadiumRecomp POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${sdl2_SOURCE_DIR}/lib/x64/SDL2.dll"
            "${CMAKE_SOURCE_DIR}/lib/rt64/src/contrib/dxc/bin/x64/dxil.dll"
            "${CMAKE_SOURCE_DIR}/lib/rt64/src/contrib/dxc/bin/x64/dxcompiler.dll"
            $<TARGET_FILE_DIR:PokemonStadiumRecomp>
    )

    target_link_libraries(PokemonStadiumRecomp PRIVATE SDL2 SDL2main Winmm.lib dbghelp.lib comdlg32.lib)
endif()

target_link_libraries(PokemonStadiumRecomp PRIVATE
    RecompiledFuncs
    librecomp
    ultramodern
    rt64
    ares_bridge
)

# Force-include all of ares_bridge.lib so the static initializer that
# installs the RSP per-instruction hook (HookInstaller in
# ares_rsp_ring.cpp) is not dropped by /OPT:REF. Without this, lld-link
# strips the unreferenced global and the hook never fires, leaving
# ares_rsp_trace_count stuck at 0 even while Ares runs.
if (MSVC)
    target_link_options(PokemonStadiumRecomp PRIVATE
        "/WHOLEARCHIVE:$<TARGET_FILE:ares_bridge>"
    )
endif()

target_include_directories(PokemonStadiumRecomp PRIVATE
    ${CMAKE_SOURCE_DIR}/lib/N64ModernRuntime/N64Recomp/ares-bridge/include
)
