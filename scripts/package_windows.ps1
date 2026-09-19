param(
  [string]$BuildDir = "build/release",
  [string]$OutDir = "dist/ArenaTES3JSON"
)
$ErrorActionPreference = "Stop"
if (-not $env:QTDIR) { throw "Set QTDIR, e.g. C:\Qt\6.8.3\msvc2022_64" }
New-Item -Force -ItemType Directory $OutDir | Out-Null
Copy-Item "$BuildDir/ArenaTES3JSON.exe" $OutDir -Force
Copy-Item "$BuildDir/ArenaTES3JSON-cli.exe" $OutDir -Force
& "$env:QTDIR/bin/windeployqt.exe" --release --no-translations "$OutDir/ArenaTES3JSON.exe"
Copy-Item README.md,LICENSE "$OutDir" -Force
Compress-Archive -Path "$OutDir/*" -DestinationPath "dist/ArenaTES3JSON-windows-x64.zip" -Force
Write-Host "Created dist/ArenaTES3JSON-windows-x64.zip"
