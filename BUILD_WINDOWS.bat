@echo off
setlocal
where cmake >nul 2>nul || (echo CMake not found & exit /b 1)
where cargo >nul 2>nul || (echo Rust/Cargo not found & exit /b 1)
if "%QTDIR%"=="" (
  echo Set QTDIR to a Qt 6 MSVC kit, for example C:\Qt\6.8.3\msvc2022_64
  exit /b 1
)
cmake -S . -B build -G "Visual Studio 17 2022" -A x64 -DCMAKE_PREFIX_PATH="%QTDIR%" || exit /b 1
cmake --build build --config Release --parallel || exit /b 1
ctest --test-dir build -C Release --output-on-failure || exit /b 1
powershell -ExecutionPolicy Bypass -File scripts\package_windows.ps1 -BuildDir build\Release -OutFile dist\ArenaTES3JSON.exe || exit /b 1
powershell -ExecutionPolicy Bypass -File scripts\verify_onefile.ps1 -DistDir dist -ExeName ArenaTES3JSON.exe || exit /b 1
echo.
echo Done: dist\ArenaTES3JSON.exe
echo One portable EXE; Qt and ArenaTES3JSON-core are bundled inside it.
