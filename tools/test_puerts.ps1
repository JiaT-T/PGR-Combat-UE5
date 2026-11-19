# 运行编辑器或独立包，核对 Puerts 原生回调与脚本环境关闭日志。
[CmdletBinding(DefaultParameterSetName = 'Editor')]
param (
    [Parameter(ParameterSetName = 'Editor')]
    [string] $EngineRoot = $env:UE_ROOT,
    [Parameter(Mandatory, ParameterSetName = 'Package')]
    [string] $PackagedExe,
    [switch] $D3D12
)

$ErrorActionPreference = 'Stop'
$projectDir = Split-Path $PSScriptRoot -Parent
$logDir = (New-Item -ItemType Directory -Force -Path (Join-Path $projectDir 'Saved/Logs')).FullName
$kind = $PSCmdlet.ParameterSetName
$timestamp = [DateTime]::UtcNow.ToString('yyyyMMdd-HHmmss-fff')
$rhi = if ($D3D12) { 'D3D12' } else { 'Null' }
$log = Join-Path $logDir "Puerts-$kind-$rhi-$timestamp.log"

if ($kind -eq 'Editor') {
    if (!$EngineRoot) { throw 'Set UE_ROOT or supply -EngineRoot.' }
    $executable = Join-Path $EngineRoot 'Engine/Binaries/Win64/UnrealEditor-Cmd.exe'
    $launchArgs = @((Join-Path $projectDir 'PGRCombat.uproject'), '-game')
} else {
    $executable = (Resolve-Path -LiteralPath $PackagedExe).Path
    $launchArgs = @()
}

$launchArgs += if ($D3D12) { @('-d3d12', '-RenderOffscreen', '-windowed', '-ResX=640', '-ResY=360') } else { @('-nullrhi') }
$launchArgs += @('-unattended', '-NoSplash', '-NoSound', '-NoAnalytics', '-NoLiveCoding',
                 '-seconds=4', "-ABSLOG=$log")
& $executable @launchArgs | Out-Null
$code = $LASTEXITCODE
if ($code -ne 0 -or !(Test-Path -LiteralPath $log)) {
    throw "Puerts $kind process failed (exit $code). Log: $log"
}

$started = @(Select-String -LiteralPath $log -SimpleMatch 'LogPGRScript: Display: Puerts bridge ready:')
$stopped = @(Select-String -LiteralPath $log -SimpleMatch 'LogPGRScript: Display: Puerts bridge stopped')
if ($started.Count -ne 1 -or $stopped.Count -ne 1 -or $started[0].LineNumber -ge $stopped[0].LineNumber) {
    throw "Expected one successful native round trip followed by shutdown. Log: $log"
}
Write-Output "Puerts $kind passed (exit $code). Log: $log"
