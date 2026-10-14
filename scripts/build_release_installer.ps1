param(
    [string]$Version = "1.0.3",
    [string]$BuildDir = "",
    [string]$OutputDir = "",
    [int]$Jobs = 6,
    [switch]$ReuseExistingArtifacts
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

function Resolve-CMakePath {
    param(
        [Parameter(Mandatory = $true)]
        [string]$RepoRoot
    )

    $localRoot = Join-Path $RepoRoot "tools\clion-cmake"
    $localCMake = Join-Path $localRoot "bin\cmake.exe"
    if (Test-Path $localCMake) {
        return $localCMake
    }

    $sourceRoots = @(
        "C:\Program Files\JetBrains\CLion 2025.3.3\bin\cmake\win\x64",
        "C:\Program Files\JetBrains\CLion 2025.2.4\bin\cmake\win\x64",
        "C:\Program Files\JetBrains\CLion\bin\cmake\win\x64"
    )

    foreach ($sourceRoot in $sourceRoots) {
        if (Test-Path (Join-Path $sourceRoot "bin\cmake.exe")) {
            Write-Host "Preparing local CMake toolchain copy..."
            Copy-Item -Path (Join-Path $sourceRoot "*") -Destination $localRoot -Recurse -Force
            return $localCMake
        }
    }

    $cmake = Get-Command cmake -ErrorAction SilentlyContinue
    if ($cmake) {
        return $cmake.Source
    }

    throw "cmake.exe not found. Install CMake or CLion bundled CMake first."
}

function Resolve-MingwRoot {
    param(
        [Parameter(Mandatory = $true)]
        [string]$RepoRoot
    )

    $localRoot = Join-Path $RepoRoot "tools\clion-mingw"
    $localCompiler = Join-Path $localRoot "bin\g++.exe"
    if (Test-Path $localCompiler) {
        return $localRoot
    }

    $sourceRoots = @(
        "C:\Program Files\JetBrains\CLion 2025.3.3\bin\mingw",
        "C:\Program Files\JetBrains\CLion 2025.2.4\bin\mingw",
        "C:\Program Files\JetBrains\CLion\bin\mingw"
    )

    foreach ($sourceRoot in $sourceRoots) {
        if (Test-Path (Join-Path $sourceRoot "bin\g++.exe")) {
            Write-Host "Preparing local MinGW toolchain copy..."
            Copy-Item -Path (Join-Path $sourceRoot "*") -Destination $localRoot -Recurse -Force
            return $localRoot
        }
    }

    throw "MinGW toolchain not found."
}

function Get-FetchContentSourceArguments {
    param(
        [Parameter(Mandatory = $true)]
        [string]$RepoRoot
    )

    $depsRoot = Join-Path $RepoRoot "cmake-build-release\_deps"
    $sourceMap = @{
        "GLFW"          = Join-Path $depsRoot "glfw-src"
        "IMGUI"         = Join-Path $depsRoot "imgui-src"
        "NLOHMANN_JSON" = Join-Path $depsRoot "nlohmann_json-src"
        "MINIZ"         = Join-Path $depsRoot "miniz-src"
        "BOX2D"         = Join-Path $depsRoot "box2d-src"
    }

    $arguments = @()
    foreach ($entry in $sourceMap.GetEnumerator()) {
        if (Test-Path $entry.Value) {
            $arguments += "-DFETCHCONTENT_SOURCE_DIR_$($entry.Key)=$($entry.Value)"
        }
    }

    return $arguments
}

function Resolve-IExpressPath {
    $candidates = @(
        "C:\Windows\System32\iexpress.exe",
        "C:\Windows\SysWOW64\iexpress.exe"
    )

    foreach ($candidate in $candidates) {
        if (Test-Path $candidate) {
            return $candidate
        }
    }

    throw "iexpress.exe not found."
}

function Invoke-Step {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Exe,
        [Parameter(Mandatory = $true)]
        [string[]]$Arguments
    )

    $global:LASTEXITCODE = 0
    & $Exe @Arguments
    $exitCode = if (Get-Variable -Name LASTEXITCODE -Scope Global -ErrorAction SilentlyContinue) {
        $global:LASTEXITCODE
    } else {
        0
    }
    if ($exitCode -ne 0) {
        throw "Command failed: $Exe $($Arguments -join ' ')"
    }
}

function Stage-ExistingReleaseArtifacts {
    param(
        [Parameter(Mandatory = $true)]
        [string]$RepoRoot,
        [Parameter(Mandatory = $true)]
        [string]$StageDir
    )

    $binDir = Join-Path $RepoRoot "cmake-build-release\bin"
    $fallbackDir = Join-Path $RepoRoot "release_package"
    $primaryExe = Join-Path $binDir "PLCSimulator.exe"

    if (-not (Test-Path $primaryExe)) {
        throw "Existing release executable not found: $primaryExe"
    }

    Copy-Item -Path (Join-Path $binDir "*") -Destination $StageDir -Recurse -Force

    foreach ($dllName in @("libgcc_s_seh-1.dll", "libstdc++-6.dll", "libwinpthread-1.dll")) {
        $targetDll = Join-Path $StageDir $dllName
        if (-not (Test-Path $targetDll)) {
            $fallbackDll = Join-Path $fallbackDir $dllName
            if (Test-Path $fallbackDll) {
                Copy-Item -LiteralPath $fallbackDll -Destination $targetDll -Force
            }
        }
    }

    foreach ($docName in @("README.md", "README_kr.md", "README_ja.md", "LICENSE")) {
        $sourceDoc = Join-Path $RepoRoot $docName
        if (Test-Path $sourceDoc) {
            Copy-Item -LiteralPath $sourceDoc -Destination (Join-Path $StageDir $docName) -Force
        }
    }
}

$repoRoot = Split-Path -Parent $PSScriptRoot
if ([string]::IsNullOrWhiteSpace($BuildDir)) {
    $BuildDir = Join-Path $repoRoot "cmake-build-release-installer"
}
if ([string]::IsNullOrWhiteSpace($OutputDir)) {
    $OutputDir = Join-Path $repoRoot "dist"
}

$iexpressPath = Resolve-IExpressPath

$stageRoot = Join-Path $OutputDir "installer-stage"
$stageDir = Join-Path $stageRoot "PLCSimulator"
$workDir = Join-Path $OutputDir "installer-work"
$payloadZip = Join-Path $OutputDir "PLCSimulator-$Version-windows-x64.zip"
$installerExe = Join-Path $OutputDir "PLCSimulator-Setup-$Version.exe"
$sedPath = Join-Path $workDir "plcsimulator-installer.sed"
$installCmdPath = Join-Path $workDir "install.cmd"
$installPs1Path = Join-Path $workDir "install.ps1"
$payloadCopyPath = Join-Path $workDir "payload.zip"

foreach ($path in @($stageRoot, $workDir)) {
    if (Test-Path $path) {
        Remove-Item -LiteralPath $path -Recurse -Force
    }
}

foreach ($path in @($OutputDir, $stageDir, $workDir)) {
    New-Item -ItemType Directory -Path $path -Force | Out-Null
}

if (Test-Path $payloadZip) {
    Remove-Item -LiteralPath $payloadZip -Force
}
if (Test-Path $installerExe) {
    Remove-Item -LiteralPath $installerExe -Force
}

if ($ReuseExistingArtifacts) {
    Write-Host "[1/5] Reusing existing release artifacts..."
    Stage-ExistingReleaseArtifacts -RepoRoot $repoRoot -StageDir $stageDir
} else {
    $cmakePath = Resolve-CMakePath -RepoRoot $repoRoot
    $mingwRoot = Resolve-MingwRoot -RepoRoot $repoRoot
    $makePath = Join-Path $mingwRoot "bin\mingw32-make.exe"
    $cCompiler = Join-Path $mingwRoot "bin\gcc.exe"
    $cxxCompiler = Join-Path $mingwRoot "bin\g++.exe"
    $rcCompiler = Join-Path $mingwRoot "bin\windres.exe"
    $fetchContentArgs = Get-FetchContentSourceArguments -RepoRoot $repoRoot

    Write-Host "[1/5] Reconfiguring release build..."
    $configureArgs = @(
        "-S", $repoRoot,
        "-B", $BuildDir,
        "-G", "MinGW Makefiles",
        "-DCMAKE_BUILD_TYPE=Release",
        "-DCMAKE_MAKE_PROGRAM=$makePath",
        "-DCMAKE_C_COMPILER=$cCompiler",
        "-DCMAKE_CXX_COMPILER=$cxxCompiler",
        "-DCMAKE_RC_COMPILER=$rcCompiler"
    ) + $fetchContentArgs
    Invoke-Step -Exe $cmakePath -Arguments $configureArgs

    Write-Host "[2/5] Building PLCSimulator release..."
    Invoke-Step -Exe $cmakePath -Arguments @("--build", $BuildDir, "--config", "Release", "--target", "PLCSimulator", "--", "-j", "$Jobs")

    Write-Host "[3/5] Installing files into staging directory..."
    Invoke-Step -Exe $cmakePath -Arguments @("--install", $BuildDir, "--config", "Release", "--prefix", $stageDir)
}

Write-Host "[4/5] Creating portable release archive..."
Compress-Archive -Path (Join-Path $stageDir "*") -DestinationPath $payloadZip -CompressionLevel Optimal
Copy-Item -LiteralPath $payloadZip -Destination $payloadCopyPath

$installCmd = @'
@echo off
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0install.ps1"
exit /b %ERRORLEVEL%
'@
Set-Content -LiteralPath $installCmdPath -Value $installCmd -Encoding ASCII

$installPs1 = @'
$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$payloadZip = Join-Path $scriptDir "payload.zip"
$targetDir = Join-Path $env:LOCALAPPDATA "Programs\PLCSimulator"
$desktopDir = [Environment]::GetFolderPath("Desktop")
$programsDir = [Environment]::GetFolderPath("Programs")
$startMenuDir = Join-Path $programsDir "PLCSimulator"
$appExe = Join-Path $targetDir "PLCSimulator.exe"

if (Test-Path $targetDir) {
    Remove-Item -LiteralPath $targetDir -Recurse -Force
}
New-Item -ItemType Directory -Path $targetDir -Force | Out-Null
Expand-Archive -LiteralPath $payloadZip -DestinationPath $targetDir -Force

if (-not (Test-Path $appExe)) {
    throw "PLCSimulator.exe was not installed correctly."
}

New-Item -ItemType Directory -Path $startMenuDir -Force | Out-Null
$shell = New-Object -ComObject WScript.Shell

$desktopShortcut = $shell.CreateShortcut((Join-Path $desktopDir "PLCSimulator.lnk"))
$desktopShortcut.TargetPath = $appExe
$desktopShortcut.WorkingDirectory = $targetDir
$desktopShortcut.IconLocation = $appExe
$desktopShortcut.Save()

$startMenuShortcut = $shell.CreateShortcut((Join-Path $startMenuDir "PLCSimulator.lnk"))
$startMenuShortcut.TargetPath = $appExe
$startMenuShortcut.WorkingDirectory = $targetDir
$startMenuShortcut.IconLocation = $appExe
$startMenuShortcut.Save()

Start-Process -FilePath $appExe | Out-Null
'@
Set-Content -LiteralPath $installPs1Path -Value $installPs1 -Encoding ASCII

$sedContent = @"
[Version]
Class=IEXPRESS
SEDVersion=3
[Options]
PackagePurpose=InstallApp
ShowInstallProgramWindow=0
HideExtractAnimation=1
UseLongFileName=1
InsideCompressed=0
CAB_FixedSize=0
CAB_ResvCodeSigning=0
RebootMode=N
InstallPrompt=%InstallPrompt%
DisplayLicense=%DisplayLicense%
FinishMessage=%FinishMessage%
TargetName=%TargetName%
FriendlyName=%FriendlyName%
AppLaunched=%AppLaunched%
PostInstallCmd=%PostInstallCmd%
AdminQuietInstCmd=%AdminQuietInstCmd%
UserQuietInstCmd=%UserQuietInstCmd%
SourceFiles=SourceFiles
[Strings]
InstallPrompt=
DisplayLicense=
FinishMessage=PLCSimulator $Version has been installed.
TargetName=$installerExe
FriendlyName=PLCSimulator $Version Setup
AppLaunched=install.cmd
PostInstallCmd=<None>
AdminQuietInstCmd=install.cmd
UserQuietInstCmd=install.cmd
FILE0="install.cmd"
FILE1="install.ps1"
FILE2="payload.zip"
[SourceFiles]
SourceFiles0=$workDir
[SourceFiles0]
%FILE0%=
%FILE1%=
%FILE2%=
"@
Set-Content -LiteralPath $sedPath -Value $sedContent -Encoding ASCII

Write-Host "[5/5] Building installer EXE..."
$iexpressProcess = Start-Process -FilePath $iexpressPath -ArgumentList @("/N", $sedPath) -Wait -PassThru
if ($iexpressProcess.ExitCode -ne 0) {
    throw "IExpress failed with exit code $($iexpressProcess.ExitCode)."
}

if (-not (Test-Path $installerExe)) {
    throw "Installer was not generated: $installerExe"
}

Write-Host ""
Write-Host "Installer build complete."
Write-Host "ZIP: $payloadZip"
Write-Host "EXE: $installerExe"
