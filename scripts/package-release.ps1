<#
.SYNOPSIS
    CodeVault (CodeJudge Local) 一键 Release 部署与便携绿色包制作脚本
.DESCRIPTION
    依据《大作业五周计划第五周》§3.3 与 §4 规范，自动收集 Qt 依赖、内嵌工具链、官方演示题包、
    样例源码及使用说明，生成即解即用、脱离系统环境变量的独立 Windows x64 发布包及 ZIP 压缩包。
.PARAMETER ExePath
    CodeVaultUI.exe 的源路径。若不指定，将自动在构建目录中搜索。
.PARAMETER TargetDir
    生成目标文件夹路径。默认为仓库根目录下的 release_dist/CodeJudgeLocal_v1.0_Windows_x64。
.PARAMETER CreateZip
    是否同步打包为 .zip 压缩文件（默认为 $true）。
#>
[CmdletBinding()]
param (
    [string]$ExePath = "",
    [string]$TargetDir = "",
    [switch]$CreateZip = $true
)

$ErrorActionPreference = "Stop"
$RepoRoot = (Resolve-Path "$PSScriptRoot/..").Path

Write-Host "==================================================" -ForegroundColor Cyan
Write-Host "   CodeVault 终期 Release 部署与发布包生成工具   " -ForegroundColor Cyan
Write-Host "==================================================" -ForegroundColor Cyan

# 1. 定位 CodeVaultUI.exe
if (-not $ExePath) {
    $Candidates = @(
        "$RepoRoot/ui/build/Desktop_Qt_6_11_2_MSVC2022_64bit_Debug/CodeVaultUI.exe",
        "$RepoRoot/ui/build/Desktop_Qt_6_11_2_MSVC2022_64bit_Release/CodeVaultUI.exe",
        "$RepoRoot/ui/build/vs2026/CodeVaultUI.exe",
        "$RepoRoot/build/CodeVaultUI.exe"
    )
    foreach ($cand in $Candidates) {
        if (Test-Path $cand) {
            $ExePath = (Resolve-Path $cand).Path
            break
        }
    }
}

if (-not $ExePath -or -not (Test-Path $ExePath)) {
    Write-Error "未找到 CodeVaultUI.exe！请先在 Qt Creator 或 CMake 中完成构建，或通过 -ExePath 参数指定。"
}

Write-Host "[1/7] 已定位可执行程序: $ExePath" -ForegroundColor Green

# 2. 设置目标输出路径
if (-not $TargetDir) {
    $TargetDir = "$RepoRoot/release_dist/CodeJudgeLocal_v1.0_Windows_x64"
}

if (Test-Path $TargetDir) {
    Write-Host "[2/7] 清理旧发布目录: $TargetDir" -ForegroundColor Yellow
    Remove-Item -Path $TargetDir -Recurse -Force
}
New-Item -Path $TargetDir -ItemType Directory -Force | Out-Null
Write-Host "[2/7] 已创建发布目录: $TargetDir" -ForegroundColor Green

# 3. 复制主程序
Copy-Item -Path $ExePath -Destination "$TargetDir/CodeVaultUI.exe" -Force
Write-Host "[3/7] 已拷贝主程序 CodeVaultUI.exe" -ForegroundColor Green

# 4. 执行 windeployqt 收集 Qt 运行依赖
$WinDeployQt = ""
$QtDeployCandidates = @(
    "D:/Qt/6.12.0/mingw_64/bin/windeployqt.exe",
    "D:/Qt/6.11.2/msvc2022_64/bin/windeployqt.exe",
    "D:/Qt/Qt/6.12.0/mingw_64/bin/windeployqt.exe",
    (Get-Command windeployqt.exe -ErrorAction SilentlyContinue | Select-Object -ExpandProperty Source -ErrorAction SilentlyContinue)
)
foreach ($cand in $QtDeployCandidates) {
    if ($cand -and (Test-Path $cand)) {
        $WinDeployQt = $cand
        break
    }
}

if ($WinDeployQt) {
    Write-Host "[4/7] 正在调用 windeployqt 自动收集 Qt 运行时依赖..." -ForegroundColor Green
    & $WinDeployQt --no-translations --compiler-runtime "$TargetDir/CodeVaultUI.exe"
} else {
    Write-Host "[4/7] 提示：未检测到 windeployqt，正在直接拷贝构建环境已有动态库..." -ForegroundColor Yellow
    $BuildDir = Split-Path $ExePath
    Get-ChildItem -Path $BuildDir -Filter "*.dll" | Copy-Item -Destination $TargetDir -Force
    if (Test-Path "$BuildDir/platforms") {
        Copy-Item -Path "$BuildDir/platforms" -Destination "$TargetDir/platforms" -Recurse -Force
    }
    if (Test-Path "$BuildDir/styles") {
        Copy-Item -Path "$BuildDir/styles" -Destination "$TargetDir/styles" -Recurse -Force
    }
}

# 5. 复制便携式离线 GCC 工具链
$ToolchainSource = ""
$ToolchainCandidates = @(
    (Join-Path (Split-Path $ExePath) "toolchain"),
    "$RepoRoot/toolchain",
    "D:/Qt/Qt/Tools/mingw1510_64"
)
foreach ($cand in $ToolchainCandidates) {
    if (Test-Path "$cand/bin/g++.exe") {
        $ToolchainSource = $cand
        break
    }
}

if ($ToolchainSource) {
    Write-Host "[5/7] 正在复制内置离线工具链: $ToolchainSource -> $TargetDir/toolchain" -ForegroundColor Green
    Copy-Item -Path $ToolchainSource -Destination "$TargetDir/toolchain" -Recurse -Force
} else {
    Write-Host "[5/7] 警告：未找到含 bin/g++.exe 的 toolchain 目录，请手动放入 $TargetDir/toolchain！" -ForegroundColor Yellow
}

# 6. 整合演示样例、说明文档与空运行环境
Write-Host "[6/7] 整合官方题包、样例源码与文档..." -ForegroundColor Green
if (Test-Path "$RepoRoot/samples") {
    Copy-Item -Path "$RepoRoot/samples" -Destination "$TargetDir/samples" -Recurse -Force
}
if (Test-Path "$RepoRoot/examples") {
    Copy-Item -Path "$RepoRoot/examples" -Destination "$TargetDir/examples" -Recurse -Force
}
if (Test-Path "$RepoRoot/使用说明.md") {
    Copy-Item -Path "$RepoRoot/使用说明.md" -Destination "$TargetDir/使用说明.md" -Force
}
if (Test-Path "$RepoRoot/licenses") {
    Copy-Item -Path "$RepoRoot/licenses" -Destination "$TargetDir/licenses" -Recurse -Force
}

# 创建规范运行所需空数据与临时工作目录
New-Item -Path "$TargetDir/data" -ItemType Directory -Force | Out-Null
New-Item -Path "$TargetDir/work" -ItemType Directory -Force | Out-Null

# 7. 打包为 ZIP 归档
if ($CreateZip) {
    $ZipPath = "$RepoRoot/release_dist/CodeJudgeLocal_v1.0_Windows_x64.zip"
    Write-Host "[7/7] 正在创建完整便携分发包: $ZipPath..." -ForegroundColor Green
    if (Test-Path $ZipPath) {
        Remove-Item -Path $ZipPath -Force
    }
    Add-Type -AssemblyName System.IO.Compression.FileSystem
    [System.IO.Compression.ZipFile]::CreateFromDirectory($TargetDir, $ZipPath, [System.IO.Compression.CompressionLevel]::Optimal, $false)
    Write-Host ">>> 打包成功！ZIP 压缩文件大小: $([Math]::Round((Get-Item $ZipPath).Length / 1MB, 2)) MB" -ForegroundColor Cyan
}

Write-Host "==================================================" -ForegroundColor Green
Write-Host "   发布目录已就绪: $TargetDir" -ForegroundColor Green
Write-Host "==================================================" -ForegroundColor Green
