param(
  [string]$BuildDir = "build/Release",
  [string]$OutDir = "dist/ArenaTES3JSON"
)
$ErrorActionPreference = "Stop"
$QtDir = $env:QTDIR
if (-not $QtDir) { $QtDir = $env:QT_ROOT_DIR }
if (-not $QtDir) { throw "Set QTDIR (local build) or QT_ROOT_DIR (CI) to the Qt MSVC kit directory" }
if (-not (Test-Path "$QtDir/bin/windeployqt.exe")) { throw "windeployqt.exe not found under Qt directory: $QtDir" }

$required = @(
  "$BuildDir/ArenaTES3JSON.exe",
  "$BuildDir/ArenaTES3JSON-cli.exe",
  "$BuildDir/ArenaTES3JSON-core.exe"
)
foreach ($file in $required) {
  if (-not (Test-Path $file)) { throw "Required executable not found: $file" }
}

if (Test-Path $OutDir) { Remove-Item $OutDir -Recurse -Force }
New-Item -Force -ItemType Directory $OutDir | Out-Null
Copy-Item "$BuildDir/ArenaTES3JSON.exe" $OutDir -Force
Copy-Item "$BuildDir/ArenaTES3JSON-cli.exe" $OutDir -Force
Copy-Item "$BuildDir/ArenaTES3JSON-core.exe" $OutDir -Force
& "$QtDir/bin/windeployqt.exe" --release --no-translations "$OutDir/ArenaTES3JSON.exe"
if ($LASTEXITCODE -ne 0) { throw "windeployqt failed with exit code $LASTEXITCODE" }
Copy-Item README.md,README_RU.md,LICENSE,THIRD_PARTY.md "$OutDir" -Force
New-Item -Force -ItemType Directory "dist" | Out-Null
Compress-Archive -Path "$OutDir/*" -DestinationPath "dist/ArenaTES3JSON-windows-x64.zip" -Force
Write-Host "Created dist/ArenaTES3JSON-windows-x64.zip"
