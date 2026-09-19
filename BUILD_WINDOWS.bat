@echo off
setlocal
if "%QTDIR%"=="" (
  echo Set QTDIR first. Example:
  echo   set QTDIR=C:\Qt\6.8.3\msvc2022_64
  exit /b 1
)

cmake -S . -B build\release -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH=%QTDIR%
if errorlevel 1 exit /b 1
cmake --build build\release --parallel
if errorlevel 1 exit /b 1
ctest --test-dir build\release --output-on-failure
if errorlevel 1 exit /b 1

echo.
echo Built:
echo   build\release\ArenaTES3JSON.exe
echo   build\release\ArenaTES3JSON-cli.exe
endlocal
