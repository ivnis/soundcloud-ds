$ErrorActionPreference = 'Stop'
$runtime = Join-Path $PSScriptRoot '.cache/cloudflared'
$statePath = Join-Path $runtime 'session.json'
$executable = Join-Path $runtime 'cloudflared.exe'
if (!(Test-Path -LiteralPath $statePath)) { Write-Host 'No project tunnel session recorded.'; return }
$state = Get-Content -LiteralPath $statePath -Raw | ConvertFrom-Json
$process = Get-Process -Id $state.pid -ErrorAction SilentlyContinue
if ($process -and $process.Path -eq $executable -and
    $process.StartTime.ToUniversalTime().Ticks.ToString() -eq $state.started) {
    Stop-Process -Id $process.Id
    $process.WaitForExit(5000) | Out-Null
}
if (Test-Path -LiteralPath $statePath) { Remove-Item -LiteralPath $statePath -ErrorAction SilentlyContinue }
$urlPath = Join-Path $runtime 'active-url.txt'
if (Test-Path -LiteralPath $urlPath) { Remove-Item -LiteralPath $urlPath -ErrorAction SilentlyContinue }
Write-Host 'Project Cloudflare tunnel stopped. The bridge is still running.'
