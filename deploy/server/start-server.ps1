param(
    [ValidateSet('test', 'soundcloud')][string]$Source = 'soundcloud',
    [string]$Track = 'https://soundcloud.com/forss/flickermood',
    [ValidateRange(1,65535)][int]$Port = 8765,
    [string]$BindAddress = '127.0.0.1'
)
$ErrorActionPreference = 'Stop'
$python = Join-Path $PSScriptRoot '.venv/Scripts/python.exe'
if (!(Test-Path -LiteralPath $python)) {
    throw 'Create .venv and install requirements-windows.txt first; see README.md.'
}
Push-Location $PSScriptRoot
try {
    $env:SC_SOURCE = $Source
    $env:SC_TRACK_URL = $Track
    $env:FFMPEG_BIN = & $python -c 'import imageio_ffmpeg; print(imageio_ffmpeg.get_ffmpeg_exe())'
    if ($LASTEXITCODE -ne 0) { throw 'Install requirements-windows.txt first.' }
    Write-Host "Bridge: http://${BindAddress}:$Port ($Source). Press Ctrl+C to stop."
    & $python -m uvicorn server.app:app --host $BindAddress --port $Port --workers 1
    if ($LASTEXITCODE -ne 0) { throw 'Bridge exited with an error.' }
} finally { Pop-Location }
