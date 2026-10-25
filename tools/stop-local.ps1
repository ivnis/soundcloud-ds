$ErrorActionPreference = 'Stop'
$runtime = Join-Path $PSScriptRoot '.cache/local'
$statePath = Join-Path $runtime 'session.json'
if (!(Test-Path -LiteralPath $statePath)) { return }
$state = Get-Content -LiteralPath $statePath -Raw | ConvertFrom-Json
$process = Get-Process -Id $state.pid -ErrorAction SilentlyContinue
# Check creation time to avoid touching an unrelated process after PID reuse.
if ($process -and $process.StartTime.ToUniversalTime().Ticks.ToString() -eq $state.started) {
    New-Item -ItemType File -Path (Join-Path $runtime 'stop') -Force | Out-Null
    if (!$process.WaitForExit(40000)) {
        throw 'Server is still shutting down. Check tools/.cache/local/server.err.log.'
    }
}
Remove-Item -LiteralPath $statePath -Force
Write-Host 'Local bridge stopped. You may close melonDS.'
