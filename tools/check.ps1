param([string]$Cxx = "g++")
$ErrorActionPreference = "Stop"
$projectRoot = Split-Path -Parent $PSScriptRoot
Push-Location $projectRoot
try {
    $python = Join-Path $projectRoot '.venv/Scripts/python.exe'
    if (!(Test-Path -LiteralPath $python)) { $python = "python" }
    if (!$env:FFMPEG_BIN) {
        $env:FFMPEG_BIN = & $python -c "import imageio_ffmpeg; print(imageio_ffmpeg.get_ffmpeg_exe())"
        if ($LASTEXITCODE -ne 0) { throw "Set FFMPEG_BIN or install requirements-dev.txt" }
    }
    & $python -m pytest -q
    if ($LASTEXITCODE -ne 0) { throw "Server tests failed" }
    New-Item -ItemType Directory -Force client/build | Out-Null
    & $Cxx -std=c++17 -Wall -Wextra -Werror -finput-charset=UTF-8 -fexec-charset=UTF-8 -Iclient/include tests/client_core.cpp client/source/http.cpp client/source/search.cpp client/source/keyboard.cpp client/source/endpoint.cpp client/source/artwork.cpp client/source/ui.cpp -static -o client/build/core_tests.exe
    if ($LASTEXITCODE -ne 0) { throw "Client tests did not compile" }
    & ./client/build/core_tests.exe
    if ($LASTEXITCODE -ne 0) { throw "Client core tests failed" }
} finally { Pop-Location }
