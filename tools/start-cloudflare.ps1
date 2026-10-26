param(
    [ValidateRange(1,65535)][int]$Port = 8765,
    [ValidateSet('quic','http2','auto')][string]$Protocol = 'quic'
)
$ErrorActionPreference = 'Stop'
$runtime = Join-Path $PSScriptRoot '.cache/cloudflared'
$executable = Join-Path $runtime 'cloudflared.exe'
$statePath = Join-Path $runtime 'session.json'
$urlPath = Join-Path $runtime 'active-url.txt'
$logPath = Join-Path $runtime 'tunnel.log'
$origin = "http://127.0.0.1:$Port"
$version = '2026.10.3'
$expectedHash = '76B2805253D0D6FFACB092A7ACFBEE99E7AF6917835C0C5410E23F3EF5052DA2'
New-Item -ItemType Directory -Path $runtime -Force | Out-Null
if (Test-Path -LiteralPath $statePath) {
    $state = Get-Content -LiteralPath $statePath -Raw | ConvertFrom-Json
    $existing = Get-Process -Id $state.pid -ErrorAction SilentlyContinue
    if ($existing -and $existing.Path -eq $executable -and
        $existing.StartTime.ToUniversalTime().Ticks.ToString() -eq $state.started) {
        Write-Host "Project tunnel is already running: $($state.url)"
        return
    }
}
$health = Invoke-RestMethod "$origin/healthz" -TimeoutSec 3
if ($health.status -ne 'ok') { throw 'The local bridge is not ready. Start the server first.' }
if (!(Test-Path -LiteralPath $executable)) {
    Write-Host "Downloading portable cloudflared $version from Cloudflare's GitHub release..."
    [Net.ServicePointManager]::SecurityProtocol = [Net.ServicePointManager]::SecurityProtocol -bor [Net.SecurityProtocolType]::Tls12
    Invoke-WebRequest -UseBasicParsing -Uri "https://github.com/cloudflare/cloudflared/releases/download/$version/cloudflared-windows-amd64.exe" -OutFile $executable
}
if ((Get-FileHash -LiteralPath $executable -Algorithm SHA256).Hash -ne $expectedHash) {
    throw 'cloudflared SHA-256 does not match the pinned official release.'
}
Write-Host "Publishing $origin through a temporary Cloudflare Quick Tunnel. Press Ctrl+C to stop."
$startedAt = Get-Date
$publicUrl = ''
$process = $null
Set-Content -LiteralPath $logPath -Value '' -Encoding UTF8
# cloudflared writes normal log messages to stderr. Treat redirected native
# output as log data rather than terminating PowerShell errors.
$ErrorActionPreference = 'Continue'
try {
    & $executable tunnel --no-autoupdate --protocol $Protocol --edge-ip-version 4 --url $origin 2>&1 | ForEach-Object {
        $line = $_.ToString()
        Write-Host $line
        Add-Content -LiteralPath $logPath -Value $line -Encoding UTF8
        if (!$process) {
            $process = Get-Process cloudflared -ErrorAction SilentlyContinue |
                Where-Object { $_.Path -eq $executable -and $_.StartTime -ge $startedAt } |
                Sort-Object StartTime -Descending | Select-Object -First 1
        }
        if ($line -match 'https://[a-z0-9-]+\.trycloudflare\.com') {
            $publicUrl = $Matches[0]
            Set-Content -LiteralPath $urlPath -Value $publicUrl -Encoding UTF8
        }
        if ($process) {
            @{pid=$process.Id; started=$process.StartTime.ToUniversalTime().Ticks.ToString(); origin=$origin; url=$publicUrl} |
                ConvertTo-Json | Set-Content -LiteralPath $statePath -Encoding UTF8
        }
    }
} finally {
    $ErrorActionPreference = 'Stop'
    if ($process) {
        $remaining = Get-Process -Id $process.Id -ErrorAction SilentlyContinue
        if ($remaining -and $remaining.Path -eq $executable -and
            $remaining.StartTime.ToUniversalTime().Ticks -eq $process.StartTime.ToUniversalTime().Ticks) {
            Stop-Process -Id $remaining.Id
        }
    }
    if (Test-Path -LiteralPath $statePath) { Remove-Item -LiteralPath $statePath -ErrorAction SilentlyContinue }
    if (Test-Path -LiteralPath $urlPath) { Remove-Item -LiteralPath $urlPath -ErrorAction SilentlyContinue }
}
