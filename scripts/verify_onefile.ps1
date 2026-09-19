param(
  [string]$DistDir = "dist",
  [string]$ExeName = "ArenaTES3JSON.exe"
)
$ErrorActionPreference = "Stop"

$RepoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
$ResolvedDist = [System.IO.Path]::GetFullPath((Join-Path $RepoRoot $DistDir))
if (-not (Test-Path $ResolvedDist)) { throw "Output directory does not exist: $ResolvedDist" }

$files = @(Get-ChildItem $ResolvedDist -File)
if ($files.Count -ne 1 -or $files[0].Name -ne $ExeName) {
  $names = ($files | ForEach-Object Name) -join ", "
  throw "Output must contain exactly one $ExeName. Found: [$names]"
}

$exe = $files[0].FullName
Write-Host "One-file artifact: $exe ($($files[0].Length) bytes)"

# ArenaTES3JSON.exe uses the Windows GUI subsystem. Invoking it with `&` from
# PowerShell does not reliably provide a synchronous native-process exit code.
# Start-Process -Wait -PassThru explicitly waits and gives us the actual code.
$process = Start-Process -FilePath $exe -ArgumentList @("--onefile-selftest") -Wait -PassThru
Write-Host "One-file self-test exit code: $($process.ExitCode)"
if ($process.ExitCode -ne 0) {
  throw "one-file runtime self-test failed with exit code $($process.ExitCode)"
}

Write-Host "One-file package verification: OK"
