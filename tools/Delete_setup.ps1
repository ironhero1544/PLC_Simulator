[CmdletBinding()]
param(
    [switch]$RemoveMsys2Root
)

$ErrorActionPreference = "Stop"

function Stop-WithMessage {
    param(
        [string]$Message,
        [int]$Code = 1
    )
    Write-Host ""
    Write-Host "[ERR ] $Message" -ForegroundColor Red
    Write-Host ""
    Write-Host "아무 키나 누르면 창을 닫습니다..."
    $null = $Host.UI.RawUI.ReadKey("NoEcho,IncludeKeyDown")
    exit $Code
}

# 관리자 권한 자동 승격 - 반드시 pwsh 사용
$principal = New-Object Security.Principal.WindowsPrincipal([Security.Principal.WindowsIdentity]::GetCurrent())
if (-not $principal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)) {
    $pwsh = (Get-Command pwsh -ErrorAction SilentlyContinue).Source
    if (-not $pwsh) {
        Stop-WithMessage "pwsh.exe를 찾지 못했습니다."
    }

    $argList = @(
        "-NoExit"
        "-NoProfile"
        "-ExecutionPolicy", "Bypass"
        "-File", $PSCommandPath
    )

    if ($RemoveMsys2Root) {
        $argList += "-RemoveMsys2Root"
    }

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

$Msys2Root = "C:\msys64"
$Bash      = Join-Path $Msys2Root "usr\bin\bash.exe"
$UcrtBin   = Join-Path $Msys2Root "ucrt64\bin"
$UsrBin    = Join-Path $Msys2Root "usr\bin"
$WingetId  = "MSYS2.MSYS2"

$Packages = @(
    "mingw-w64-ucrt-x86_64-gcc"
    "mingw-w64-ucrt-x86_64-verilator"
    "make"
)

function Invoke-MsysBash {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Command
    )

    if (-not (Test-Path $Bash)) {
        return $false
    }

    & $Bash -lc $Command
    return ($LASTEXITCODE -eq 0)
}

function Remove-PathEntryFromUser {
    param([string]$TargetPath)

    $current = [Environment]::GetEnvironmentVariable("Path", "User")
    if ([string]::IsNullOrWhiteSpace($current)) {
        return
    }

    $target = $TargetPath.Trim().TrimEnd('\')
    $parts = $current.Split(';', [System.StringSplitOptions]::RemoveEmptyEntries) |
        ForEach-Object { $_.Trim() } |
        Where-Object { $_ -ne "" }

    $filtered = $parts | Where-Object {
        $_.TrimEnd('\') -ine $target
    }

    [Environment]::SetEnvironmentVariable("Path", ($filtered -join ';'), "User")
}

Write-Info "[1/5] 툴 패키지 제거 중..."

if (Test-Path $Bash) {
    foreach ($pkg in $Packages) {
        Write-Info "패키지 확인: $pkg"
        $exists = Invoke-MsysBash "pacman -Q $pkg > /dev/null 2>&1"
        if ($exists) {
            $removed = Invoke-MsysBash "pacman -Rns --noconfirm $pkg"
            if ($removed) {
                Write-Ok "제거 완료: $pkg"
            } else {
                Write-WarnMsg "제거 실패 또는 일부만 제거됨: $pkg"
            }
        }
        else {
            Write-Info "이미 없음: $pkg"
        }
    }
}
else {
    Write-WarnMsg "bash.exe가 없어 pacman 패키지 제거는 건너뜁니다"
}

Write-Host ""

Write-Info "[2/5] User PATH 정리 중..."
try {
    Remove-PathEntryFromUser -TargetPath $UcrtBin
    Remove-PathEntryFromUser -TargetPath $UsrBin
    Write-Ok "User PATH 정리 완료"
}
catch {
    Write-WarnMsg "User PATH 정리 중 오류가 있었지만 계속 진행합니다"
    Write-Host "       $($_.Exception.Message)" -ForegroundColor DarkYellow
}
Write-Host ""

Write-Info "[3/5] 현재 세션 PATH 정리 중..."
try {
    $sessionParts = $env:Path.Split(';', [System.StringSplitOptions]::RemoveEmptyEntries) |
        ForEach-Object { $_.Trim() } |
        Where-Object { $_ -ne "" }

    $ucrtNorm = $UcrtBin.TrimEnd('\')
    $usrNorm  = $UsrBin.TrimEnd('\')

    $sessionFiltered = $sessionParts | Where-Object {
        $_.TrimEnd('\') -ine $ucrtNorm -and $_.TrimEnd('\') -ine $usrNorm
    }

    $env:Path = ($sessionFiltered -join ';')
    Write-Ok "현재 세션 PATH 정리 완료"
}
catch {
    Write-WarnMsg "현재 세션 PATH 정리 중 오류가 있었지만 계속 진행합니다"
    Write-Host "       $($_.Exception.Message)" -ForegroundColor DarkYellow
}
Write-Host ""

Write-Info "[4/5] MSYS2 winget 삭제 시도 중..."
if (Get-Command winget -ErrorAction SilentlyContinue) {
    try {
        winget list --id $WingetId --exact | Out-Host

        winget uninstall `
            --id $WingetId `
            --exact `
            --all-versions `
            --disable-interactivity `
            --accept-source-agreements | Out-Host

        if ($LASTEXITCODE -eq 0) {
            Write-Ok "winget 삭제 완료"
        }
        else {
            Write-WarnMsg "winget 삭제 실패 (exit $LASTEXITCODE)"
        }
    }
    catch {
        Write-WarnMsg "winget 삭제가 안 됐습니다"
        Write-Host "       $($_.Exception.Message)" -ForegroundColor DarkYellow
    }
}
else {
    Write-WarnMsg "winget이 없어 MSYS2 삭제는 건너뜁니다"
}
Write-Host ""

Write-Info "[5/5] 잔여 폴더 정리..."
if ($RemoveMsys2Root) {
    if (Test-Path $Msys2Root) {
        try {
            Remove-Item $Msys2Root -Recurse -Force -ErrorAction SilentlyContinue
            Write-Ok "잔여 폴더 삭제 완료: $Msys2Root"
        }
        catch {
            Write-WarnMsg "잔여 폴더 삭제 중 오류가 있었습니다"
        }
    }
    else {
        Write-Info "삭제할 폴더가 없습니다: $Msys2Root"
    }
}
else {
    Write-Info "잔여 폴더까지 지우려면 -RemoveMsys2Root 옵션을 사용하세요"
}
Write-Host ""

Write-Ok "정리 작업이 끝났습니다."
Write-Host "아무 키나 누르면 창을 닫습니다..."
$null = $Host.UI.RawUI.ReadKey("NoEcho,IncludeKeyDown")