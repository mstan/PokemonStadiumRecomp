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
file(GLOB FUNC_C_SOURCES ${CMAKE_SOURCE_DIR}/generated/*.c)
target_sources(RecompiledFuncs PRIVATE ${FUNC_C_SOURCES})

# ---- Runner executable ------------------------------------------------------
add_executable(PokemonStadiumRecomp
    src/main/main.cpp
    src/main/rt64_render_context.cpp
    extras.c
    # Generated lookup table (provides get_entrypoint_address etc).
    generated/lookup.cpp
    # Borrow Zelda64Recomp's RSP recompiled outputs (standard libultra
    # audio + JPEG microcodes; same versions used by Pokemon Stadium-era games).
    ${CMAKE_SOURCE_DIR}/../Zelda64Recomp/rsp/aspMain.cpp
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

if (MSVC)
    target_link_options(PokemonStadiumRecomp PRIVATE /OPT:NOICF)
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

    target_link_libraries(PokemonStadiumRecomp PRIVATE SDL2 SDL2main Winmm.lib)
endif()

target_link_libraries(PokemonStadiumRecomp PRIVATE
    RecompiledFuncs
    librecomp
    ultramodern
    rt64
)
