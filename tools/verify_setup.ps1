[CmdletBinding()]
param()

$ErrorActionPreference = "Stop"

[Console]::InputEncoding  = [System.Text.UTF8Encoding]::new()
[Console]::OutputEncoding = [System.Text.UTF8Encoding]::new()
$OutputEncoding = [System.Text.UTF8Encoding]::new()
chcp 65001 > $null

function Write-Info($msg)    { Write-Host "[INFO] $msg" -ForegroundColor Cyan }
function Write-Ok($msg)      { Write-Host "[ OK ] $msg" -ForegroundColor Green }
function Write-WarnMsg($msg) { Write-Host "[WARN] $msg" -ForegroundColor Yellow }
function Write-Err($msg)     { Write-Host "[ERR ] $msg" -ForegroundColor Red }

$Msys2Root    = "C:\msys64"
$Bash         = Join-Path $Msys2Root "usr\bin\bash.exe"
$UcrtBin      = Join-Path $Msys2Root "ucrt64\bin"
$UsrBin       = Join-Path $Msys2Root "usr\bin"

$GccExe       = Join-Path $UcrtBin "gcc.exe"
$MakeExe      = Join-Path $UsrBin  "make.exe"

$HasError = $false

function Fail-Step($msg) {
    $script:HasError = $true
    Write-Err $msg
}

function Try-Run {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Label,
        [Parameter(Mandatory = $true)]
        [scriptblock]$Script,
        [switch]$Fatal
    )

    try {
        & $Script
        if ($LASTEXITCODE -eq 0) {
            Write-Ok "$Label 성공"
            return $true
        }
        else {
            if ($Fatal) {
                Fail-Step "$Label 실패 (exit $LASTEXITCODE)"
            }
            else {
                Write-WarnMsg "$Label 실패 (exit $LASTEXITCODE)"
            }
            return $false
        }
    }
    catch {
        if ($Fatal) {
            Fail-Step "$Label 예외: $($_.Exception.Message)"
        }
        else {
            Write-WarnMsg "$Label 예외: $($_.Exception.Message)"
        }
        return $false
    }
}

function Get-ResolvedCommandPath {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Name
    )

    $cmd = Get-Command $Name -ErrorAction SilentlyContinue
    if ($null -eq $cmd) {
        return $null
    }

    if ($cmd.Source) { return $cmd.Source }
    if ($cmd.Path)   { return $cmd.Path }
    return $null
}

Write-Info "[1/6] 설치 경로 확인"
if (Test-Path $Msys2Root) { Write-Ok "MSYS2 root 존재: $Msys2Root" } else { Fail-Step "MSYS2 root 없음: $Msys2Root" }
if (Test-Path $Bash)      { Write-Ok "bash.exe 존재: $Bash" }         else { Fail-Step "bash.exe 없음: $Bash" }
Write-Host ""

Write-Info "[2/6] 고정 경로 실행 파일 확인"
if (Test-Path $GccExe)  { Write-Ok "gcc.exe 존재: $GccExe" }  else { Fail-Step "gcc.exe 없음: $GccExe" }
if (Test-Path $MakeExe) { Write-Ok "make.exe 존재: $MakeExe" } else { Fail-Step "make.exe 없음: $MakeExe" }

$ResolvedVerilator = Get-ResolvedCommandPath "verilator"
if ($ResolvedVerilator) {
    Write-Ok "verilator 명령 해석 경로: $ResolvedVerilator"
}
else {
    Write-WarnMsg "verilator 명령 경로를 아직 찾지 못했습니다"
}
Write-Host ""

Write-Info "[3/6] 절대경로 실행 확인"
Try-Run -Label "gcc 절대경로 실행" -Fatal { & $GccExe --version }
Try-Run -Label "make 절대경로 실행" -Fatal { & $MakeExe --version }

if ($ResolvedVerilator) {
    Try-Run -Label "verilator 해석 경로 실행" -Fatal { & $ResolvedVerilator --version }
}
else {
    Fail-Step "verilator 경로를 찾지 못해 실행 확인 불가"
}
Write-Host ""

Write-Info "[4/6] PATH 등록 확인"
$machinePath = [Environment]::GetEnvironmentVariable("Path", "Machine")
$userPath    = [Environment]::GetEnvironmentVariable("Path", "User")
$allPath     = @($machinePath, $userPath) -join ";"

$ucrtInPath = $allPath -split ';' | Where-Object { $_.Trim().TrimEnd('\') -ieq $UcrtBin.TrimEnd('\') }
$usrInPath  = $allPath -split ';' | Where-Object { $_.Trim().TrimEnd('\') -ieq $UsrBin.TrimEnd('\') }

if ($ucrtInPath) { Write-Ok "UCRT64 bin PATH 등록됨" } else { Write-WarnMsg "UCRT64 bin PATH 미등록" }
if ($usrInPath)  { Write-Ok "usr bin PATH 등록됨"    } else { Write-WarnMsg "usr bin PATH 미등록" }
Write-Host ""

Write-Info "[5/6] 현재 셸 이름 기반 실행 확인"
Try-Run -Label "gcc PATH 실행"       { gcc --version }
Try-Run -Label "verilator PATH 실행" { verilator --version }
Try-Run -Label "make PATH 실행"      { make --version }
Write-Host ""

Write-Info "[6/6] MSYS2 bash 내부 확인"
if (Test-Path $Bash) {
    Try-Run -Label "MSYS2 bash 환경 확인" {
        & $Bash -lc 'echo MSYSTEM=$MSYSTEM'
        & $Bash -lc 'test -x /ucrt64/bin/gcc && echo gcc_ok || echo gcc_missing'
        & $Bash -lc 'command -v verilator >/dev/null 2>&1 && echo verilator_ok || echo verilator_missing'
        & $Bash -lc 'test -x /usr/bin/make && echo make_ok || echo make_missing'
    }
}
Write-Host ""

if ($HasError) {
    Write-Err "검사 결과: 실제 설치 또는 실행에 문제 있음"
    exit 1
}
else {
    Write-Ok "검사 결과: 설치 정상"
    exit 0
}