cmake_minimum_required(VERSION 3.20)

# Use the classic flag-based MSVC runtime selection (CMP0091 OLD) so the static
# CRT can be forced reliably under direct clang-cl + Ninja. Must be set before
# project(), which initializes the per-config flag variables we rewrite below.
# See the static-CRT block further down for the rationale.
if (WIN32)
    cmake_policy(SET CMP0091 OLD)
endif()

project(PokemonStadiumRecomp C CXX)

set(CMAKE_C_STANDARD 17)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang" AND CMAKE_CXX_SIMULATE_ID STREQUAL "MSVC")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /EHsc -Wno-everything /W4")
endif()
if (WIN32)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -D_DISABLE_CONSTEXPR_MUTEX_CONSTRUCTOR")
endif()
if (CMAKE_VERSION VERSION_GREATER_EQUAL "3.24.0")
    cmake_policy(SET CMP0135 NEW)
endif()

# Statically link the MSVC C/C++ runtime so the shipped executable does not
# depend on the Visual C++ redistributable being installed (no MSVCP140.dll /
# VCRUNTIME140.dll to bundle — the #1 cause of "missing DLL" errors on a clean
# machine).
#
# The CMAKE_MSVC_RUNTIME_LIBRARY abstraction (CMP0091 NEW) is not reliably
# honored for the direct clang-cl + Ninja configuration this project uses, so we
# use the classic, bulletproof approach: with CMP0091 OLD (set before project()
# above), the runtime flag lives in CMAKE_*_FLAGS_<CONFIG>; rewrite the
# dynamic-CRT flag (-MD) to the static one (/MT) in every flag variable. Because
# dependencies inherit these flag variables, every target links the static CRT.
if (WIN32)
    foreach(flag_var
            CMAKE_C_FLAGS   CMAKE_C_FLAGS_DEBUG   CMAKE_C_FLAGS_RELEASE
            CMAKE_C_FLAGS_RELWITHDEBINFO   CMAKE_C_FLAGS_MINSIZEREL
            CMAKE_CXX_FLAGS CMAKE_CXX_FLAGS_DEBUG CMAKE_CXX_FLAGS_RELEASE
            CMAKE_CXX_FLAGS_RELWITHDEBINFO CMAKE_CXX_FLAGS_MINSIZEREL)
        string(REGEX REPLACE "[-/]MDd" "/MTd" ${flag_var} "${${flag_var}}")
        string(REGEX REPLACE "[-/]MD"  "/MT"  ${flag_var} "${${flag_var}}")
    endforeach()
endif()

set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

# On Windows the bundled FreeType (lib/freetype-windows-binaries) is located via
# our own lib/FindFreetype.cmake; put lib/ on the module path so both RmlUi's
# build and our find_package(Freetype) below resolve to it.
if (WIN32)
    set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/lib/")
endif()

# Release/windowed build: no console window. Off by default (the console is handy
# for dev — stderr logs). Configure with -DPSR_NO_CONSOLE=ON for the release exe.
option(PSR_NO_CONSOLE "Build a windowed release with no console window" OFF)
option(N64_COSIM "Per-VI-frame differential co-simulation harness" OFF)

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

# ---- RmlUi + lunasvg (UI framework for the SS Anne launcher) ----------------
# Vendored forks under lib/ (version-matched to the rt64/N64MR fork lineage,
# Zelda64Recomp@ab677e7). lunasvg must come first so RmlUi's SVG plugin links
# it statically.
set(BUILD_SHARED_LIBS OFF)
set(LUNASVG_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
add_subdirectory(${CMAKE_SOURCE_DIR}/lib/lunasvg)
set(RMLUI_SVG_PLUGIN ON CACHE BOOL "" FORCE)
set(RMLUI_TESTS_ENABLED OFF CACHE BOOL "" FORCE)
add_subdirectory(${CMAKE_SOURCE_DIR}/lib/RmlUi)
target_compile_definitions(rmlui_core PRIVATE LUNASVG_BUILD_STATIC)

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

# NOTE: the register-quit/GB-Tower TRACE_ENTRY/RETURN hooks (PKMNSTADIUM_TRACE_HOOKS)
# were removed 2026-06 — Issues #7/#11 are resolved and the per-function [jrdiag]
# stderr spew was the dominant cost of the launcher->game boot stutter. The engine
# still emits TRACE_ENTRY()/TRACE_RETURN() in generated/ (game.toml trace_mode=true),
# but include/trace.h compiles them to no-ops without this define. A future regen
# with trace_mode=false purges the emitted calls entirely.
#
# Opt-in re-enable for crash RE (issues #9/#16 gallery null-vtable): turning
# PSR_TRACE_HOOKS ON defines PKMNSTADIUM_TRACE_HOOKS=1 for the recompiled funcs
# so trace_recent (the guest function-entry ring) records again. The [jrdiag]
# stderr block is now gated to 6 specific function names, so there is no flood.
# Default OFF — release builds stay no-op. Pair with the enlarged TRACE_RING_CAP
# in extras.c so the ring spans multiple frames, not <1 frame.
option(PSR_TRACE_HOOKS "Compile the recompiled funcs with TRACE_ENTRY ring hooks (crash RE)" OFF)
if(PSR_TRACE_HOOKS)
    target_compile_definitions(RecompiledFuncs PRIVATE PKMNSTADIUM_TRACE_HOOKS=1)
    message(STATUS "PSR_TRACE_HOOKS=ON — recompiled funcs record the trace_recent ring")
endif()

# Issue #9 geo probe: compile ONLY funcs_85.c (which defines process_geo_layout)
# with the TRACE_ENTRY hook so extras.c can observe each process_geo_layout entry
# and log its segptr (a1) + caller — splitting Case A (bad reload: first call gets
# a graph-pool segptr) from Case B (double-processing: 2nd call gets the overwritten
# pool ptr). One-file recompile; default OFF. (TRACE_ENTRY in funcs_85.c expands to
# the ring hook only when PKMNSTADIUM_TRACE_HOOKS is defined for that TU.)
option(PSR_GEO_PROBE "Trace-hook funcs_85.c to log process_geo_layout segptrs (#9 RE)" OFF)
if(PSR_GEO_PROBE)
    set_source_files_properties(${CMAKE_SOURCE_DIR}/generated/funcs_85.c
        PROPERTIES COMPILE_DEFINITIONS PKMNSTADIUM_TRACE_HOOKS=1)
    message(STATUS "PSR_GEO_PROBE=ON — funcs_85.c traces process_geo_layout segptrs")
endif()

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
    src/main/gb_audio.cpp
    src/main/input_bindings.cpp
    src/main/audio_host_probe.cpp
    extras.c
    # Generated lookup table (provides get_entrypoint_address etc).
    generated/lookup.cpp
    # Pokemon Stadium's own aspMain (recompiled from baserom @ 0x68020, 0xC60 bytes).
    ${CMAKE_SOURCE_DIR}/rsp/aspMain.cpp
    # GB Tower display/copy microcode from fragment 1 (selected by ucode boot signature).
    ${CMAKE_SOURCE_DIR}/rsp/gbTowerMain.cpp
    ${CMAKE_SOURCE_DIR}/rsp/gbTowerColorMain.cpp
    # njpgdspMain is the standard libultra JPEG ucode (vendored from Zelda64Recomp's recompiled output).
    ${CMAKE_SOURCE_DIR}/rsp/njpgdspMain.cpp

    # ---- SS Anne UI overlay (Phase 0 seam proof) ---------------------------
    # Minimal RmlUi<->RT64 overlay + the game-agnostic recompui renderer +
    # RmlUi's SDL platform backend. The full launcher (Phase 1) adds the rest
    # of src/ui/ as it gets adapted from Zelda64 to SS Anne.
    src/main/ui_seam.cpp
    ${CMAKE_SOURCE_DIR}/src/ui/ui_renderer.cpp
    ${CMAKE_SOURCE_DIR}/lib/RmlUi/Backends/RmlUi_Platform_SDL.cpp
)

if(N64_COSIM)
    target_sources(PokemonStadiumRecomp PRIVATE src/main/cosim_harness.cpp)
    target_compile_definitions(PokemonStadiumRecomp PRIVATE N64_COSIM=1)
    target_compile_definitions(RecompiledFuncs PRIVATE N64_COSIM=1)
    if(TARGET librecomp)
        target_compile_definitions(librecomp PUBLIC N64_COSIM=1)
    endif()
    if(TARGET ultramodern)
        target_compile_definitions(ultramodern PUBLIC N64_COSIM=1)
    endif()
    message(STATUS "N64_COSIM=ON - co-sim TCP protocol and runtime context hooks enabled")
endif()

target_include_directories(PokemonStadiumRecomp PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src/main
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
    # ---- SS Anne UI (RmlUi) include paths ----------------------------------
    ${CMAKE_SOURCE_DIR}/src/ui
    ${CMAKE_SOURCE_DIR}/src/ui/core
    ${CMAKE_SOURCE_DIR}/src/ui/util
    ${CMAKE_SOURCE_DIR}/lib/RmlUi/Include
    ${CMAKE_SOURCE_DIR}/lib/RmlUi/Backends
    ${CMAKE_SOURCE_DIR}/lib/GamepadMotionHelpers
    ${CMAKE_SOURCE_DIR}/lib/freetype-windows-binaries/include
    ${CMAKE_SOURCE_DIR}/lib/rt64/src/contrib/nativefiledialog-extended/src/include
    # Generated Interface{VS,PS} shader headers land here.
    ${CMAKE_BINARY_DIR}/shaders
    ${CMAKE_CURRENT_BINARY_DIR}
)

target_compile_options(PokemonStadiumRecomp PRIVATE
    -fno-strict-aliasing
    -fms-extensions
    -march=nehalem  # Required by RT64's RSP simulator (SSSE3, SSE4.1, SSE4.2)
)

# The UI sources (ui_renderer.cpp, RmlUi backend) include RT64's hlslpp via
# Windows headers; without NOMINMAX the min/max macros break hlslpp's min()/
# max() declarations. Zelda64Recomp sets this globally; we scope it to the
# runner target's sources so the rt64/RmlUi sub-builds are untouched.
target_compile_definitions(PokemonStadiumRecomp PRIVATE NOMINMAX)

# Emit PDBs in Release for the runner + librecomp/ultramodern so the SEH
# crash filter's stack walk produces real symbol names. clang-cl needs
# /Z7 (embed CodeView in objects) on the compiler side and /DEBUG on the
# linker side; /OPT:REF + /OPT:NOICF keep dead-code stripping but stop
# IDENTICAL COMDAT folding from collapsing different functions to the
# same address (which would corrupt the stack-walk symbol mapping).
if (MSVC)
    target_compile_options(PokemonStadiumRecomp PRIVATE /Z7)
    # The 1007 generated funcs dominate compile memory/time. Their /Z7 CodeView
    # only feeds the host SEH stack-walk, which is already unsymbolized for the
    # recompiled code and irrelevant to the guest trace_recent ring. In a
    # PSR_TRACE_HOOKS build we diagnose from the guest ring, so skip /Z7 here —
    # it roughly halves per-object memory and avoids clang-cl thrashing/OOM.
    if (NOT PSR_TRACE_HOOKS)
        target_compile_options(RecompiledFuncs   PRIVATE /Z7)
    endif()
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

    target_link_libraries(PokemonStadiumRecomp PRIVATE SDL2 SDL2main Winmm.lib dbghelp.lib comdlg32.lib ole32.lib avrt.lib)
endif()

target_link_libraries(PokemonStadiumRecomp PRIVATE
    RecompiledFuncs
    librecomp
    ultramodern
    rt64
    ares_bridge
    # ---- SS Anne UI (RmlUi) ------------------------------------------------
    RmlUi::Core
    RmlUi::Debugger
    lunasvg
    nfd
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

# Expose the real-Ares oracle's debug-server commands only when the real bridge
# is configured. Without this define the runner references none of the Ares
# trace symbols (which live in the real ares_bridge), so the default
# (placeholder) build links cleanly with the static CRT.
if (WITH_ARES_BRIDGE)
    target_compile_definitions(PokemonStadiumRecomp PRIVATE PSR_WITH_ARES=1)
endif()

# Windowed (no-console) release. SDL2main supplies WinMain for the WINDOWS
# subsystem, which calls our SDL_main (main() is renamed by SDL.h), so no /ENTRY
# override is needed. stderr still routes to a redirected handle if one is set,
# so logging/diagnostics aren't lost when launched from a terminal.
if (WIN32 AND PSR_NO_CONSOLE)
    target_link_options(PokemonStadiumRecomp PRIVATE /SUBSYSTEM:WINDOWS)
endif()

target_include_directories(PokemonStadiumRecomp PRIVATE
    ${CMAKE_SOURCE_DIR}/lib/N64ModernRuntime/N64Recomp/ares-bridge/include
)

# ---- FreeType (RmlUi font engine) ------------------------------------------
# Resolved via lib/FindFreetype.cmake (static, bundled). RmlUi's own build also
# finds it through the module path set near the top of this file.
find_package(Freetype REQUIRED)
target_include_directories(PokemonStadiumRecomp PRIVATE ${FREETYPE_INCLUDE_DIRS})
target_link_libraries(PokemonStadiumRecomp PRIVATE ${FREETYPE_LIBRARIES})

# ---- Interface shader compilation (RmlUi overlay) --------------------------
# build_vertex_shader / build_pixel_shader come from lib/rt64's CMakeLists and
# need the DXC option vars + DXC path set in this scope (rt64's own CMake does
# not export them). Mirrors Zelda64Recomp@ab677e7's top-level shader setup.
set(DXC_COMMON_OPTS "-I${PROJECT_SOURCE_DIR}/src")
set(DXC_DXIL_OPTS "-Wno-ignored-attributes")
set(DXC_SPV_OPTS "-spirv" "-fspv-target-env=vulkan1.0" "-fvk-use-dx-layout")
set(DXC_PS_OPTS "${DXC_COMMON_OPTS}" "-E" "PSMain" "-T ps_6_0" "-D DYNAMIC_RENDER_PARAMS")
set(DXC_VS_OPTS "${DXC_COMMON_OPTS}" "-E" "VSMain" "-T vs_6_0" "-D DYNAMIC_RENDER_PARAMS" "-fvk-invert-y")
if (WIN32)
    set(DXC "${PROJECT_SOURCE_DIR}/lib/rt64/src/contrib/dxc/bin/x64/dxc.exe")
elseif (APPLE)
    set(DXC "DYLD_LIBRARY_PATH=${PROJECT_SOURCE_DIR}/lib/rt64/src/contrib/dxc/lib/arm64" "${PROJECT_SOURCE_DIR}/lib/rt64/src/contrib/dxc/bin/arm64/dxc-macos")
else()
    set(DXC "LD_LIBRARY_PATH=${PROJECT_SOURCE_DIR}/lib/rt64/src/contrib/dxc/lib/x64" "${PROJECT_SOURCE_DIR}/lib/rt64/src/contrib/dxc/bin/x64/dxc-linux")
endif()

build_vertex_shader(PokemonStadiumRecomp "shaders/InterfaceVS.hlsl" "shaders/InterfaceVS.hlsl")
build_pixel_shader(PokemonStadiumRecomp "shaders/InterfacePS.hlsl" "shaders/InterfacePS.hlsl")

# ---- UI assets (fonts, etc.) next to the executable ------------------------
add_custom_command(TARGET PokemonStadiumRecomp POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_SOURCE_DIR}/assets"
        "$<TARGET_FILE_DIR:PokemonStadiumRecomp>/assets"
)
