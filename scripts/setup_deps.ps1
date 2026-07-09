# Fetch header-only third-party libraries (Eigen 3.4.0 + stb single headers) into third_party/.
# PowerShell counterpart of setup_deps.sh for Windows users.
$ErrorActionPreference = "Stop"
$tp = Join-Path (Split-Path -Parent $PSScriptRoot) "third_party"
New-Item -ItemType Directory -Force -Path $tp | Out-Null

if (-not (Test-Path "$tp\stb_image_write.h")) {
    Write-Host "Fetching stb_image_write.h ..."
    Invoke-WebRequest "https://raw.githubusercontent.com/nothings/stb/master/stb_image_write.h" -OutFile "$tp\stb_image_write.h"
}
if (-not (Test-Path "$tp\stb_image.h")) {
    Write-Host "Fetching stb_image.h ..."
    Invoke-WebRequest "https://raw.githubusercontent.com/nothings/stb/master/stb_image.h" -OutFile "$tp\stb_image.h"
}
if (-not (Test-Path "$tp\eigen")) {
    Write-Host "Fetching Eigen 3.4.0 ..."
    Invoke-WebRequest "https://gitlab.com/libeigen/eigen/-/archive/3.4.0/eigen-3.4.0.tar.gz" -OutFile "$tp\eigen.tar.gz"
    tar -xzf "$tp\eigen.tar.gz" -C $tp
    Move-Item "$tp\eigen-3.4.0" "$tp\eigen"
    Remove-Item "$tp\eigen.tar.gz"
}
Write-Host "Dependencies ready in $tp"
