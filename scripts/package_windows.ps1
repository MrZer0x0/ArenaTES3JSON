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
$PayloadCab = Join-Path $WorkDir "payload.cab"
$DdfFile = Join-Path $WorkDir "payload.ddf"
$LauncherBuild = Join-Path $WorkDir "launcher-build"

$QtDir = $env:QTDIR
if (-not $QtDir) { $QtDir = $env:QT_ROOT_DIR }
if (-not $QtDir) { throw "Set QTDIR (local build) or QT_ROOT_DIR (CI) to the Qt MSVC kit directory" }
if (-not (Test-Path "$QtDir/bin/windeployqt.exe")) { throw "windeployqt.exe not found under Qt directory: $QtDir" }
if (-not (Get-Command makecab.exe -ErrorAction SilentlyContinue)) { throw "makecab.exe is required to create the native one-file payload" }

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

# Qt remains dynamically linked inside the payload, while users receive one EXE.
& "$QtDir/bin/windeployqt.exe" --release --no-translations (Join-Path $StageDir "ArenaTES3JSON.exe")
if ($LASTEXITCODE -ne 0) { throw "windeployqt failed with exit code $LASTEXITCODE" }

foreach ($notice in @("LICENSE", "THIRD_PARTY.md")) {
  $source = Join-Path $RepoRoot $notice
  if (Test-Path $source) { Copy-Item $source $StageDir -Force }
}

# Build a CAB payload. The launcher extracts it natively with SetupAPI, so the
# finished EXE uses native Windows extraction on the user's PC.
$ddf = New-Object System.Collections.Generic.List[string]
$ddf.Add('.OPTION EXPLICIT')
$ddf.Add('.Set Cabinet=ON')
$ddf.Add('.Set Compress=ON')
$ddf.Add('.Set CompressionType=LZX')
$ddf.Add('.Set CompressionMemory=21')
$ddf.Add('.Set MaxDiskSize=0')
$ddf.Add('.Set MaxCabinetSize=0')
$ddf.Add('.Set MaxDiskFileCount=0')
$ddf.Add('.Set FolderFileCountThreshold=0')
$ddf.Add('.Set FolderSizeThreshold=0')
$ddf.Add('.Set CabinetFileCountThreshold=0')
$ddf.Add('.Set CabinetNameTemplate=payload.cab')
$ddf.Add('.Set DiskDirectoryTemplate=.')

$currentDir = $null
$files = Get-ChildItem -LiteralPath $StageDir -Recurse -File | Sort-Object FullName
if ($files.Count -eq 0) { throw "Runtime staging directory is empty: $StageDir" }

foreach ($file in $files) {
  $relative = [System.IO.Path]::GetRelativePath($StageDir, $file.FullName).Replace('/', '\')
  $dir = [System.IO.Path]::GetDirectoryName($relative)
  if ([string]::IsNullOrEmpty($dir)) { $dir = '' }
  if ($dir -ne $currentDir) {
    $ddf.Add(".Set DestinationDir=$dir")
    $currentDir = $dir
  }
  if ($file.FullName.Contains('"')) { throw "Unsupported quote in payload path: $($file.FullName)" }
  $ddf.Add(('"{0}"' -f $file.FullName))
}

# makecab's DDF parser is legacy; CI paths are ASCII and this keeps it maximally compatible.
$ddf | Set-Content -LiteralPath $DdfFile -Encoding Ascii
Push-Location $WorkDir
try {
  & makecab.exe /F $DdfFile | Out-Host
  if ($LASTEXITCODE -ne 0) { throw "makecab failed with exit code $LASTEXITCODE" }
} finally {
  Pop-Location
}
if (-not (Test-Path $PayloadCab)) { throw "CAB payload was not produced: $PayloadCab" }

$PayloadId = (Get-FileHash -Algorithm SHA256 $PayloadCab).Hash.ToLowerInvariant()

$LauncherSource = Join-Path $RepoRoot "scripts/onefile"
$IconFile = Join-Path $RepoRoot "resources/ArenaTES3JSON.ico"
if (-not (Test-Path $IconFile)) { throw "Application icon not found: $IconFile" }

& cmake -S $LauncherSource -B $LauncherBuild -G "Visual Studio 17 2022" -A x64 `
    "-DPAYLOAD_CAB=$PayloadCab" "-DPAYLOAD_ID=$PayloadId" "-DAPP_VERSION=0.4.0" "-DICON_FILE=$IconFile"
if ($LASTEXITCODE -ne 0) { throw "Failed to configure one-file launcher" }

& cmake --build $LauncherBuild --config Release --parallel
if ($LASTEXITCODE -ne 0) { throw "Failed to build one-file launcher" }

$LauncherExe = Join-Path $LauncherBuild "Release/ArenaTES3JSON.exe"
if (-not (Test-Path $LauncherExe)) { throw "One-file launcher was not produced: $LauncherExe" }
Copy-Item $LauncherExe $OutFile -Force

$size = (Get-Item $OutFile).Length
$cabSize = (Get-Item $PayloadCab).Length
Write-Host "Created one-file build: $OutFile ($size bytes)"
Write-Host "Native CAB payload: $cabSize bytes"
Write-Host "Payload SHA-256: $PayloadId"

Remove-Item $WorkDir -Recurse -Force
