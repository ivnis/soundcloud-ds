param([string]$PythonExecutable = '')
$ErrorActionPreference = 'Stop'
if (!(Test-Path -LiteralPath (Join-Path $PSScriptRoot 'server/requirements.txt'))) {
    throw 'Extract the entire server ZIP first. Do not run this file from inside the ZIP.'
}
$candidates = @()
if ($PythonExecutable) {
    $candidates += @{Executable=$PythonExecutable; Arguments=@()}
} else {
    $launcher = Get-Command py -ErrorAction SilentlyContinue
    if ($launcher) { $candidates += @{Executable=$launcher.Source; Arguments=@('-3.13')} }
    $interpreter = Get-Command python -ErrorAction SilentlyContinue
    if ($interpreter) { $candidates += @{Executable=$interpreter.Source; Arguments=@()} }
}
$selected = $null
foreach ($candidate in $candidates) {
    $arguments = $candidate.Arguments
    $version = & $candidate.Executable @arguments -c 'import sys; print(sys.version.split()[0].rsplit(chr(46),1)[0])' 2>$null
    if ($LASTEXITCODE -eq 0 -and $version -eq '3.13') { $selected=$candidate; break }
}
if (!$selected) { throw 'Python 3.13 was not found. Install it from python.org, then run Setup Server.cmd again.' }
$python = Join-Path $PSScriptRoot '.venv/Scripts/python.exe'
if (!(Test-Path -LiteralPath $python)) {
    Write-Host 'Creating the server Python environment...'
    $arguments = $selected.Arguments
    & $selected.Executable @arguments -m venv (Join-Path $PSScriptRoot '.venv')
    if ($LASTEXITCODE -ne 0) { throw 'Could not create .venv. Check that this folder is writable.' }
}
Push-Location $PSScriptRoot
try {
    Write-Host 'Installing server dependencies. Internet access is required...'
    & $python -m pip install --disable-pip-version-check -r requirements-windows.txt
    if ($LASTEXITCODE -ne 0) { throw 'Dependency installation failed. Check the connection and run Setup Server.cmd again.' }
    & $python -c 'import fastapi, uvicorn, soundcloud, PIL, imageio_ffmpeg'
    if ($LASTEXITCODE -ne 0) { throw 'Dependencies could not be loaded. See the error above.' }
    Write-Host 'Setup complete. Double-click Start Server.cmd to run the bridge.'
} finally { Pop-Location }
