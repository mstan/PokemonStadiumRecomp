@echo off
:: Trace-enabled build helper — same toolchain as build_ssanne.bat but with
:: -DPSR_TRACE_HOOKS=ON so the recompiled funcs record the trace_recent ring
:: (guest function-entry history) for crash RE (issues #9/#16 gallery null-vtable).
:: Logs to _cfg_trace.log (configure) and _build_trace.log (build).
setlocal

set "PROJ=%~dp0"
if "%PROJ:~-1%"=="\" set "PROJ=%PROJ:~0,-1%"

set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "%VSWHERE%" set "VSWHERE=%ProgramFiles%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "%VSWHERE%" ( echo vswhere not found & exit /b 1 )

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
if not exist "%VSINSTALL%\VC\Tools\Llvm\x64\bin\clang-cl.exe" ( echo clang-cl not found & exit /b 1 )

echo === CONFIGURE (PSR_TRACE_HOOKS=ON) ===
"%CMAKE%" -S "%PROJ%" -B "%PROJ%\build" -G Ninja -DCMAKE_C_COMPILER=clang-cl -DCMAKE_CXX_COMPILER=clang-cl -DCMAKE_BUILD_TYPE=Release -DPSR_NO_CONSOLE=ON -DWITH_ARES_BRIDGE=OFF -DPSR_TRACE_HOOKS=ON > "%PROJ%\_cfg_trace.log" 2>&1
set CFG_RC=%errorlevel%
echo configure rc=%CFG_RC%
if not "%CFG_RC%"=="0" ( echo CONFIGURE FAILED rc=%CFG_RC% & exit /b %CFG_RC% )

echo === BUILD (throttled: -j 6, see launcher for low priority) ===
:: Cap parallel jobs so 16 simultaneous clang-cl /O2 don't thrash the machine
:: (they did at the default -j16, especially alongside a second build session).
:: Launch this .bat via `start /low` to keep all descendants at idle priority.
"%CMAKE%" --build "%PROJ%\build" --target PokemonStadiumRecomp -- -j 6 > "%PROJ%\_build_trace.log" 2>&1
set BLD_RC=%errorlevel%
echo build rc=%BLD_RC%
exit /b %BLD_RC%
