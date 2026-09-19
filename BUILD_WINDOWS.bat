@echo off
setlocal

if "%QTDIR%"=="" if not "%QT_ROOT_DIR%"=="" set "QTDIR=%QT_ROOT_DIR%"
if "%QTDIR%"=="" (
  echo Set QTDIR first. Example:
  echo   set QTDIR=C:\Qt\6.8.3\msvc2022_64
  exit /b 1
)

if not exist "%QTDIR%\bin\qmake.exe" (
  echo QTDIR does not look like a Qt MSVC kit:
  echo   %QTDIR%
  exit /b 1
)

rem Force the Visual Studio 2022 x64 generator. Do not use an arbitrary
rem compiler from PATH: a MinGW compiler cannot be mixed with win64_msvc2022_64.
cmake -S . -B build\windows-msvc -G "Visual Studio 17 2022" -A x64 -DCMAKE_PREFIX_PATH="%QTDIR%"
if errorlevel 1 exit /b 1

cmake --build build\windows-msvc --config Release --parallel
if errorlevel 1 exit /b 1

ctest --test-dir build\windows-msvc -C Release --output-on-failure
if errorlevel 1 exit /b 1

echo.
echo Built:
echo   build\windows-msvc\Release\ArenaTES3JSON.exe
echo   build\windows-msvc\Release\ArenaTES3JSON-cli.exe
endlocal
