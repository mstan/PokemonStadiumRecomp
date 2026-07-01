@echo off
:: Co-simulation build helper — same toolchain as build_ssanne.bat, but:
::   * -DN64_COSIM=ON        (per-VI-frame co-sim harness; see COSIM.md)
::   * THROTTLED so it never starves the machine:
::       - job cap via PSR_BUILD_JOBS (default 6)
::       - compiler children run at BELOW-NORMAL priority
:: Ares oracle stays OFF for Tier 0 (T1-T6); flip WITH_ARES_BRIDGE=ON at T7.
:: Logs: _cfg_cosim.log (configure), _build_cosim.log (build).
setlocal

set "PROJ=%~dp0"
if "%PROJ:~-1%"=="\" set "PROJ=%PROJ:~0,-1%"

:: Job cap (override with:  set PSR_BUILD_JOBS=N  before calling)
if "%PSR_BUILD_JOBS%"=="" set "PSR_BUILD_JOBS=6"

set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "%VSWHERE%" set "VSWHERE=%ProgramFiles%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "%VSWHERE%" ( echo vswhere not found - install Visual Studio 2022 with C++ tools & exit /b 1 )

set "VSINSTALL="
for /f "usebackq delims=" %%i in (`"%VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "VSINSTALL=%%i"
if "%VSINSTALL%"=="" ( echo No VS install with C++ tools found & exit /b 1 )

set "VCVARS=%VSINSTALL%\VC\Auxiliary\Build\vcvars64.bat"
if not exist "%VCVARS%" ( echo vcvars64.bat not found at "%VCVARS%" & exit /b 1 )

set "CMAKE=%VSINSTALL%\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
if not exist "%CMAKE%" set "CMAKE=cmake"

call "%VCVARS%" >nul
if errorlevel 1 ( echo VCVARS FAILED & exit /b 1 )

set "PATH=%VSINSTALL%\VC\Tools\Llvm\x64\bin;%PATH%"
if not exist "%VSINSTALL%\VC\Tools\Llvm\x64\bin\clang-cl.exe" (
    echo clang-cl not found - install the "C++ Clang tools for Windows" VS component
    exit /b 1
)

echo === CONFIGURE (N64_COSIM=ON) ===
"%CMAKE%" -S "%PROJ%" -B "%PROJ%\build" -G Ninja -DCMAKE_C_COMPILER=clang-cl -DCMAKE_CXX_COMPILER=clang-cl -DCMAKE_BUILD_TYPE=Release -DPSR_NO_CONSOLE=ON -DWITH_ARES_BRIDGE=OFF -DWITH_DEV_DIVERGENCE=OFF -DN64_COSIM=ON > "%PROJ%\_cfg_cosim.log" 2>&1
set CFG_RC=%errorlevel%
echo configure rc=%CFG_RC%
if not "%CFG_RC%"=="0" ( echo CONFIGURE FAILED rc=%CFG_RC% & exit /b %CFG_RC% )

echo === BUILD (jobs=%PSR_BUILD_JOBS%, below-normal priority) ===
:: start /belownormal /b /wait launches the build at reduced priority (children
:: inherit it); the inner cmd /c owns the log redirection. -- -j N caps Ninja.
start "" /belownormal /b /wait cmd /c ""%CMAKE%" --build "%PROJ%\build" --target PokemonStadiumRecomp -- -j %PSR_BUILD_JOBS% > "%PROJ%\_build_cosim.log" 2>&1"
set BLD_RC=%errorlevel%
echo build rc=%BLD_RC%
exit /b %BLD_RC%
