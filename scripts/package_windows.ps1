param(
  [string]$BuildDir = "build/Release",
  [string]$OutFile = "dist/ArenaTES3JSON.exe"
)
$ErrorActionPreference = "Stop"

$RepoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
$BuildDir = [System.IO.Path]::GetFullPath((Join-Path $RepoRoot $BuildDir))
$OutFile = [System.IO.Path]::GetFullPath((Join-Path $RepoRoot $OutFile))
$DistDir = Split-Path -Parent $OutFile
$WorkDir = Join-Path $DistDir ".onefile"
$StageDir = Join-Path $WorkDir "payload"
$PayloadZip = Join-Path $WorkDir "payload.zip"
$LauncherBuild = Join-Path $WorkDir "launcher-build"

$QtDir = $env:QTDIR
if (-not $QtDir) { $QtDir = $env:QT_ROOT_DIR }
if (-not $QtDir) { throw "Set QTDIR (local build) or QT_ROOT_DIR (CI) to the Qt MSVC kit directory" }
if (-not (Test-Path "$QtDir/bin/windeployqt.exe")) { throw "windeployqt.exe not found under Qt directory: $QtDir" }

$GuiExe = Join-Path $BuildDir "ArenaTES3JSON.exe"
$CoreExe = Join-Path $BuildDir "ArenaTES3JSON-core.exe"
foreach ($file in @($GuiExe, $CoreExe)) {
  if (-not (Test-Path $file)) { throw "Required executable not found: $file" }
}

if (Test-Path $WorkDir) { Remove-Item $WorkDir -Recurse -Force }
New-Item -Force -ItemType Directory $StageDir | Out-Null
New-Item -Force -ItemType Directory $DistDir | Out-Null

Copy-Item $GuiExe (Join-Path $StageDir "ArenaTES3JSON.exe") -Force
Copy-Item $CoreExe (Join-Path $StageDir "ArenaTES3JSON-core.exe") -Force

# Keep Qt dynamically linked inside the one-file package. This avoids static-Qt
# licensing/relinking complications while the user still receives one EXE.
& "$QtDir/bin/windeployqt.exe" --release --no-translations (Join-Path $StageDir "ArenaTES3JSON.exe")
if ($LASTEXITCODE -ne 0) { throw "windeployqt failed with exit code $LASTEXITCODE" }

# Include project notices inside the extracted runtime directory.
foreach ($notice in @("LICENSE", "THIRD_PARTY.md")) {
  $source = Join-Path $RepoRoot $notice
  if (Test-Path $source) { Copy-Item $source $StageDir -Force }
}

if (Test-Path $PayloadZip) { Remove-Item $PayloadZip -Force }
Compress-Archive -Path (Join-Path $StageDir "*") -DestinationPath $PayloadZip -CompressionLevel Optimal -Force
$PayloadId = (Get-FileHash -Algorithm SHA256 $PayloadZip).Hash.ToLowerInvariant()

$LauncherSource = Join-Path $RepoRoot "scripts/onefile"
& cmake -S $LauncherSource -B $LauncherBuild -G "Visual Studio 17 2022" -A x64 `
    "-DPAYLOAD_ZIP=$PayloadZip" "-DPAYLOAD_ID=$PayloadId" "-DAPP_VERSION=0.3.2"
if ($LASTEXITCODE -ne 0) { throw "Failed to configure one-file launcher" }

& cmake --build $LauncherBuild --config Release --parallel
if ($LASTEXITCODE -ne 0) { throw "Failed to build one-file launcher" }

$LauncherExe = Join-Path $LauncherBuild "Release/ArenaTES3JSON.exe"
if (-not (Test-Path $LauncherExe)) { throw "One-file launcher was not produced: $LauncherExe" }
Copy-Item $LauncherExe $OutFile -Force

$size = (Get-Item $OutFile).Length
Write-Host "Created one-file build: $OutFile ($size bytes)"
Write-Host "Payload SHA-256: $PayloadId"

# The final dist output is intentionally one EXE. Build staging is disposable.
Remove-Item $WorkDir -Recurse -Force
