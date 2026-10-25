param(
    [string]$MakeExecutable = "make",
    [string]$ShellExecutable = "sh",
    [string]$WonderfulRoot = $env:WONDERFUL_TOOLCHAIN,
    [string]$BlocksDSRoot = $env:BLOCKSDS
)
$ErrorActionPreference = "Stop"
$projectRoot = Split-Path -Parent $PSScriptRoot
if (!$WonderfulRoot) {
    $cachedRoot = Join-Path $PSScriptRoot '.cache/wonderful'
    if (Test-Path -LiteralPath $cachedRoot) { $WonderfulRoot = $cachedRoot }
}
if (!$WonderfulRoot) { throw "Set WONDERFUL_TOOLCHAIN or use Wonderful Toolchain Shell with make -C client" }
if (!$BlocksDSRoot) { $BlocksDSRoot = Join-Path $WonderfulRoot 'thirdparty/blocksds/core' }
if (!(Test-Path -LiteralPath (Join-Path $BlocksDSRoot 'sys/default_makefiles/rom_arm9/Makefile'))) {
    throw "BlocksDS SDK not found: pass -BlocksDSRoot"
}
$originalPath = $env:PATH
try {
    $env:PATH = (Join-Path $WonderfulRoot 'bin') + ';' + $env:PATH
    if (Test-Path -LiteralPath $ShellExecutable) {
        $env:PATH = (Split-Path -Parent $ShellExecutable) + ';' + $env:PATH
    }
    & $MakeExecutable -C (Join-Path $projectRoot 'client') `
        "BLOCKSDS=$($BlocksDSRoot.Replace('\', '/'))" `
        "WONDERFUL_TOOLCHAIN=$($WonderfulRoot.Replace('\', '/'))" `
        "SHELL=$($ShellExecutable.Replace('\', '/'))"
    if ($LASTEXITCODE -ne 0) { throw "NDS build failed" }
} finally { $env:PATH = $originalPath }
