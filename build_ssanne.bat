@echo off
:: SS Anne build helper — vcvars64 + cmake(Ninja, clang-cl) configure + build.
:: Logs to _cfg_ssanne.log (configure) and _build_ssanne.log (build).
::
:: Portable: the project directory is this script's own location, Visual Studio
:: is located via vswhere, and CMake is taken from PATH (falling back to the one
:: bundled with Visual Studio). No machine-specific paths are baked in.
setlocal

:: Project dir = directory containing this script (strip trailing backslash).
set "PROJ=%~dp0"
if "%PROJ:~-1%"=="\" set "PROJ=%PROJ:~0,-1%"

:: Locate Visual Studio via vswhere (installed with the VS Installer at a fixed
:: location), then derive vcvars64.bat from the install path.
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "%VSWHERE%" set "VSWHERE=%ProgramFiles%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "%VSWHERE%" ( echo vswhere not found - install Visual Studio 2022 with C++ tools & exit /b 1 )

set "VSINSTALL="
for /f "usebackq delims=" %%i in (`"%VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "VSINSTALL=%%i"
if "%VSINSTALL%"=="" ( echo No VS install with C++ tools found & exit /b 1 )

set "VCVARS=%VSINSTALL%\VC\Auxiliary\Build\vcvars64.bat"
if not exist "%VCVARS%" ( echo vcvars64.bat not found at "%VCVARS%" & exit /b 1 )

:: Prefer the CMake bundled with Visual Studio. A cmake from elsewhere on PATH
:: (e.g. an MSYS2/devkitPro install) can shadow it and break this Ninja +
:: clang-cl configuration, so only fall back to PATH cmake if the VS one is absent.
set "CMAKE=%VSINSTALL%\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
if not exist "%CMAKE%" set "CMAKE=cmake"

call "%VCVARS%" >nul
if errorlevel 1 ( echo VCVARS FAILED & exit /b 1 )

:: This project builds with clang-cl (the C/generated code relies on clang
:: warning flags like -Wno-unused-parameter that MSVC cl.exe rejects). vcvars64
:: does not put the LLVM tools on PATH, so prepend them and select clang-cl
:: explicitly — otherwise a fresh configure defaults to cl.exe and fails.
set "PATH=%VSINSTALL%\VC\Tools\Llvm\x64\bin;%PATH%"
if not exist "%VSINSTALL%\VC\Tools\Llvm\x64\bin\clang-cl.exe" (
    echo clang-cl not found - install the "C++ Clang tools for Windows" VS component
    exit /b 1
)

echo === CONFIGURE ===
:: -DPSR_NO_CONSOLE=ON => windowed release exe, no console window. stderr still
:: routes to a redirected handle, so launching with a redirect still captures logs.
:: -DCMAKE_BUILD_TYPE=Release => optimized build (Ninja is single-config, so the
::   type must be pinned explicitly). -DWITH_ARES_BRIDGE=OFF => the ares oracle
::   (a dev-only divergence-checking tool, prebuilt against the dynamic CRT) is
::   left out of the shipped build: it isn't needed to play, and including it
::   would force the dynamic MSVC runtime, defeating the static-CRT linkage that
::   makes the release self-contained. The placeholder ares worker idles
::   harmlessly. Devs who need the oracle reconfigure with -DWITH_ARES_BRIDGE=ON.
:: -DWITH_DEV_DIVERGENCE=OFF => keep the dev-only static-vs-interp divergence
::   tooling (force-interpret a fragment) out of shipped builds. Passed
::   explicitly so a stale cache left ON by a debugging session can't leak the
::   tooling into a release. Devs enable it with -DWITH_DEV_DIVERGENCE=ON.
"%CMAKE%" -S "%PROJ%" -B "%PROJ%\build" -G Ninja -DCMAKE_C_COMPILER=clang-cl -DCMAKE_CXX_COMPILER=clang-cl -DCMAKE_BUILD_TYPE=Release -DPSR_NO_CONSOLE=ON -DWITH_ARES_BRIDGE=OFF -DWITH_DEV_DIVERGENCE=OFF > "%PROJ%\_cfg_ssanne.log" 2>&1
set CFG_RC=%errorlevel%
echo configure rc=%CFG_RC%
if not "%CFG_RC%"=="0" ( echo CONFIGURE FAILED rc=%CFG_RC% & exit /b %CFG_RC% )

echo === BUILD ===
"%CMAKE%" --build "%PROJ%\build" --target PokemonStadiumRecomp > "%PROJ%\_build_ssanne.log" 2>&1
set BLD_RC=%errorlevel%
echo build rc=%BLD_RC%
exit /b %BLD_RC%
