param(
    [ValidateSet('test', 'soundcloud')][string]$Source = 'soundcloud',
    [string]$Track = 'https://soundcloud.com/forss/flickermood',
    [switch]$ServerOnly
)
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
$runtime = Join-Path $PSScriptRoot '.cache/local'
$python = Join-Path $projectRoot '.venv/Scripts/python.exe'
$emulator = Join-Path $PSScriptRoot 'melonds/melonDS.exe'
$rom = Join-Path $projectRoot 'client/soundcloud_ds.nds'
$statePath = Join-Path $runtime 'session.json'
if (!(Test-Path -LiteralPath $python)) { throw 'Install server/requirements-dev.txt into .venv first.' }
if (!$ServerOnly -and !(Test-Path -LiteralPath $emulator)) { throw 'melonDS is missing from tools/melonds.' }
if (!(Test-Path -LiteralPath $rom)) { throw 'Build client/soundcloud_ds.nds first.' }
New-Item -ItemType Directory -Path $runtime -Force | Out-Null
$reuse = $false
if (Test-Path -LiteralPath $statePath) {
    $state = Get-Content -LiteralPath $statePath -Raw | ConvertFrom-Json
    $process = Get-Process -Id $state.pid -ErrorAction SilentlyContinue
    if ($process -and $process.StartTime.ToUniversalTime().Ticks.ToString() -eq $state.started) {
        try {
            $health = Invoke-RestMethod 'http://127.0.0.1:8765/healthz' -TimeoutSec 2
            $reuse = $state.source -eq $Source -and $state.track -eq $Track -and $health.status -eq 'ok'
        } catch { $reuse = $false }
        if (!$reuse) { & (Join-Path $PSScriptRoot 'stop-local.ps1') }
    }
}
if (!$reuse) {
    $stopPath = Join-Path $runtime 'stop'
    if (Test-Path -LiteralPath $stopPath) { Remove-Item -LiteralPath $stopPath }
    # Abort if this port belongs to a service outside our session.
    $listener = Get-NetTCPConnection -State Listen -LocalPort 8765 -ErrorAction SilentlyContinue
    if ($listener) { throw 'Port 8765 is already in use by another process.' }
    if ($Track.Contains('"')) { throw 'Invalid track URL.' }
    $arguments = @('-u', ('"{0}"' -f (Join-Path $PSScriptRoot 'local_bridge.py')),
                   '--source', $Source, '--track', ('"{0}"' -f $Track),
                   '--stop-file', ('"{0}"' -f $stopPath))
    $process = Start-Process -FilePath $python -ArgumentList $arguments `
        -WorkingDirectory $projectRoot -WindowStyle Hidden -PassThru `
        -RedirectStandardOutput (Join-Path $runtime 'server.log') `
        -RedirectStandardError (Join-Path $runtime 'server.err.log')
    @{pid=$process.Id; started=$process.StartTime.ToUniversalTime().Ticks.ToString(); source=$Source; track=$Track} |
        ConvertTo-Json | Set-Content -LiteralPath $statePath -Encoding UTF8
    $ready = $false
    for ($attempt = 0; $attempt -lt 60; $attempt++) {
        if ($process.HasExited) { throw 'Bridge exited. See tools/.cache/local/server.err.log.' }
        try {
            $health = Invoke-RestMethod 'http://127.0.0.1:8765/healthz' -TimeoutSec 1
            if ($health.status -eq 'ok') { $ready = $true; break }
        } catch { }
        Start-Sleep -Milliseconds 200
    }
    if (!$ready) { throw 'Bridge did not become ready. Check server.err.log.' }
}
Write-Host "Local server ready: http://127.0.0.1:8765 ($Source)"
if (!$ServerOnly) {
    $alreadyOpen = Get-Process -Name melonDS -ErrorAction SilentlyContinue |
        Where-Object { $_.Path -eq $emulator }
    if (!$alreadyOpen) {
        $emulatorProcess = Start-Process -FilePath $emulator -ArgumentList ('"{0}"' -f $rom) `
            -WorkingDirectory (Split-Path -Parent $emulator) -PassThru `
            -RedirectStandardOutput (Join-Path $runtime 'emulator.log') `
            -RedirectStandardError (Join-Path $runtime 'emulator.err.log')
        if ($emulatorProcess.WaitForExit(1000)) {
            throw 'melonDS exited. Check tools/.cache/local/emulator.err.log.'
        }
    }
    Write-Host 'Use DS A to connect to melonAP. Tap the search field or press DS Y to search.'
    Write-Host 'Tap a result to play. DS X switches Search/Player, DS B stops. L/R control volume.'
    Write-Host 'Player: Left/Right seek 5 seconds, Up/Down change tracks. Drag the progress bar to seek.'
    Write-Host 'The gear in the upper-right opens server and Wi-Fi settings. Covers load in the background.'
    Write-Host 'Your existing melonDS input mapping is preserved. See docs/LOCAL_TEST.md.'
}
