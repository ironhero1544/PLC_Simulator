[CmdletBinding()]
param()

$ErrorActionPreference = "Stop"

# 관리자 권한 자동 승격 - 반드시 pwsh 사용
$principal = New-Object Security.Principal.WindowsPrincipal([Security.Principal.WindowsIdentity]::GetCurrent())
if (-not $principal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)) {
    $pwsh = (Get-Command pwsh -ErrorAction SilentlyContinue).Source
    if (-not $pwsh) {
        Write-Host "[ERR ] pwsh.exe를 찾지 못했습니다." -ForegroundColor Red
        exit 1
    }

    $argList = @(
        "-NoProfile"
        "-ExecutionPolicy", "Bypass"
        "-File", $PSCommandPath
    )

    Start-Process -FilePath $pwsh -Verb RunAs -ArgumentList $argList
    exit
}

[Console]::InputEncoding  = [System.Text.UTF8Encoding]::new()
[Console]::OutputEncoding = [System.Text.UTF8Encoding]::new()
$OutputEncoding = [System.Text.UTF8Encoding]::new()
chcp 65001 > $null

function Write-Info($msg)    { Write-Host "[INFO] $msg" -ForegroundColor Cyan }
function Write-Ok($msg)      { Write-Host "[ OK ] $msg" -ForegroundColor Green }
function Write-WarnMsg($msg) { Write-Host "[WARN] $msg" -ForegroundColor Yellow }
function Write-Err($msg)     { Write-Host "[ERR ] $msg" -ForegroundColor Red }

$Msys2Root   = "C:\msys64"
$Bash        = Join-Path $Msys2Root "usr\bin\bash.exe"
$PacmanConf  = Join-Path $Msys2Root "etc\pacman.conf"
$WingetId    = "MSYS2.MSYS2"

$UcrtBin     = Join-Path $Msys2Root "ucrt64\bin"
$UsrBin      = Join-Path $Msys2Root "usr\bin"

$Packages = @(
    "mingw-w64-ucrt-x86_64-gcc"
    "mingw-w64-ucrt-x86_64-verilator"
    "make"
)

if (-not (Get-Command winget -ErrorAction SilentlyContinue)) {
    Write-Err "winget이 없습니다. App Installer를 설치한 뒤 다시 실행하세요."
    exit 1
}

function Invoke-MsysBash {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Command
    )

    if (-not (Test-Path $Bash)) {
        Write-Err "bash.exe를 찾을 수 없습니다: $Bash"
        exit 1
    }

    & $Bash -lc $Command
    $code = $LASTEXITCODE
    if ($code -ne 0) {
        Write-Err "명령 실패 (exit $code)"
        Write-Host "       $Command" -ForegroundColor DarkGray
        exit $code
    }
}

function Repair-OrInstall-Msys2 {
    if (Test-Path $Bash) {
        Write-Ok "MSYS2 실행 파일이 이미 존재합니다"
        return
    }

    Write-WarnMsg "bash.exe가 없습니다. MSYS2 강제 재설치를 시도합니다."

    if (Test-Path $Msys2Root) {
        Remove-Item $Msys2Root -Recurse -Force -ErrorAction SilentlyContinue
    }

    winget install `
        --id $WingetId `
        --exact `
        --force `
        --accept-source-agreements `
        --accept-package-agreements | Out-Host

    if (-not (Test-Path $Bash)) {
        Write-Err "MSYS2 설치 후에도 bash.exe를 찾지 못했습니다: $Bash"
        exit 1
    }

    Write-Ok "MSYS2 설치 완료"
}

function Add-PathEntryUnique {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Entry,
        [ValidateSet("User","Machine")]
        [string]$Scope = "Machine"
    )

    if (-not (Test-Path $Entry)) {
        Write-WarnMsg "PATH에 추가할 경로가 없습니다: $Entry"
        return
    }

    $current = [Environment]::GetEnvironmentVariable("Path", $Scope)
    $parts = @()

    if (-not [string]::IsNullOrWhiteSpace($current)) {
        $parts = $current.Split(';') | Where-Object { -not [string]::IsNullOrWhiteSpace($_) }
    }

    $normalizedEntry = $Entry.Trim().TrimEnd('\')
    $exists = $false

    foreach ($p in $parts) {
        if ($p.Trim().TrimEnd('\') -ieq $normalizedEntry) {
            $exists = $true
            break
        }
    }

    if (-not $exists) {
        $newPath = if ([string]::IsNullOrWhiteSpace($current)) {
            $Entry
        } else {
            "$Entry;$current"
        }
        [Environment]::SetEnvironmentVariable("Path", $newPath, $Scope)
        Write-Ok "PATH 추가됨 [$Scope]: $Entry"
    }
    else {
        Write-Info "이미 PATH에 있습니다 [$Scope]: $Entry"
    }

    # 현재 세션에도 즉시 반영
    if (($env:Path.Split(';') | ForEach-Object { $_.Trim().TrimEnd('\') }) -notcontains $normalizedEntry) {
        $env:Path = "$Entry;$env:Path"
    }
}

Write-Info "[1/6] MSYS2 설치 확인 중..."
Repair-OrInstall-Msys2
Write-Host ""

Write-Info "[2/6] MSYS2 환경 설정 중..."
$env:CHERE_INVOKING = "1"
$env:MSYSTEM        = "UCRT64"

$PacmanLock = Join-Path $Msys2Root "var\lib\pacman\db.lck"
if (Test-Path $PacmanLock) {
    Remove-Item $PacmanLock -Force -ErrorAction SilentlyContinue
    Write-WarnMsg "이전 pacman lock 파일 제거"
}
Write-Ok "환경 설정 완료"
Write-Host ""

Write-Info "[3/6] pacman 설정 조정 중..."
if (Test-Path $PacmanConf) {
    $content = Get-Content $PacmanConf -Raw
    if ($content -match '(?m)^CheckSpace\s*$') {
        $content = $content -replace '(?m)^CheckSpace\s*$', '#CheckSpace'
        Set-Content $PacmanConf -Value $content
        Write-Ok "CheckSpace 비활성화 완료"
    }
}
Write-Host ""

Write-Info "[4/6] pacman 전체 업데이트 중..."
Invoke-MsysBash "pacman -Syu --noconfirm"
Invoke-MsysBash "pacman -Su --noconfirm"
Write-Ok "pacman 업데이트 완료"
Write-Host ""

Write-Info "[5/6] GCC + Verilator 설치 중..."
$pkgList = $Packages -join " "
Invoke-MsysBash "pacman -S --noconfirm --needed $pkgList"
Write-Ok "패키지 설치 완료"
Write-Host ""

Write-Info "[6/6] PATH 등록 중..."
Add-PathEntryUnique -Entry $UcrtBin -Scope Machine
Add-PathEntryUnique -Entry $UsrBin  -Scope Machine
Write-Ok "PATH 등록 완료"
Write-Host ""

Write-Info "설치 결과 확인 중..."
& (Join-Path $UcrtBin "gcc.exe") --version
& (Join-Path $UcrtBin "verilator.exe") --version
& (Join-Path $UsrBin "make.exe") --version

Write-Host ""
Write-Ok "완료: GCC와 Verilator가 설치되었습니다."
Write-Host "새 터미널을 열면 gcc / verilator / make를 바로 사용할 수 있습니다." -ForegroundColor Green