@echo off
:: SS Anne build helper — vcvars64 + cmake(Ninja, clang-cl) configure + build.
:: Logs to _cfg_ssanne.log (configure) and _build_ssanne.log (build).
setlocal
set "VCVARS=C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"
set "CMAKE=C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
set "PROJ=F:\Projects\n64recomp\PokemonStadiumRecomp"

call "%VCVARS%" >nul
if errorlevel 1 ( echo VCVARS FAILED & exit /b 1 )

echo === CONFIGURE ===
:: -DPSR_NO_CONSOLE=ON => windowed release exe, no console window. stderr still
:: routes to a redirected handle, so launching with a redirect still captures logs.
"%CMAKE%" -S "%PROJ%" -B "%PROJ%\build" -G Ninja -DPSR_NO_CONSOLE=ON > "%PROJ%\_cfg_ssanne.log" 2>&1
set CFG_RC=%errorlevel%
echo configure rc=%CFG_RC%
if not "%CFG_RC%"=="0" ( echo CONFIGURE FAILED rc=%CFG_RC% & exit /b %CFG_RC% )

echo === BUILD ===
"%CMAKE%" --build "%PROJ%\build" --target PokemonStadiumRecomp > "%PROJ%\_build_ssanne.log" 2>&1
set BLD_RC=%errorlevel%
echo build rc=%BLD_RC%
exit /b %BLD_RC%
