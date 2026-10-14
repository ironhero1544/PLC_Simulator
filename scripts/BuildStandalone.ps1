param(
  [Parameter(Mandatory = $true)][string]$PackageZip,
  [string]$MakeNsis = 'C:\Program Files (x86)\NSIS\makensis.exe',
  [string]$Version = '2.0.0'
)

$ErrorActionPreference = 'Stop'
$package_path = (Resolve-Path -LiteralPath $PackageZip).Path
$repo_root = Split-Path -Parent $PSScriptRoot
$output_dir = Split-Path -Parent $package_path
$payload_hash = (Get-FileHash -LiteralPath $package_path -Algorithm SHA256).Hash.ToLowerInvariant()
$cache_id = "$Version-$($payload_hash.Substring(0, 16))"
$staging_dir = Join-Path $repo_root "tmp/standalone-$cache_id"
New-Item -ItemType Directory -Path $staging_dir -Force | Out-Null
Expand-Archive -LiteralPath $package_path -DestinationPath $staging_dir -Force
$payload_dir = Join-Path $staging_dir ([IO.Path]::GetFileNameWithoutExtension($package_path))
if (-not (Test-Path -LiteralPath (Join-Path $payload_dir 'PLCSimulator.exe'))) {
  throw 'The CPack ZIP must contain PLCSimulator.exe in its package directory.'
}
$output_path = Join-Path $output_dir "PLCSimulator-$Version-win64-standalone.exe"
$script_path = Join-Path $staging_dir 'standalone.nsi'

$nsis_script = @'
Unicode true
!include "FileFunc.nsh"
Name "PLCSimulator Standalone"
OutFile "@OUTPUT@"
Icon "@ICON@"
RequestExecutionLevel user
SilentInstall silent
AutoCloseWindow true
SetCompressor /SOLID lzma
VIProductVersion "@VERSION@.0"
VIAddVersionKey /LANG=1033 "ProductName" "PLCSimulator Standalone"
VIAddVersionKey /LANG=1033 "FileDescription" "PLCSimulator single-file launcher"
VIAddVersionKey /LANG=1033 "FileVersion" "@VERSION@"
VIAddVersionKey /LANG=1033 "ProductVersion" "@VERSION@"
VIAddVersionKey /LANG=1033 "LegalCopyright" "PLCSimulator contributors; GPL-3.0"
Var Parameters
Var ExtractOnly

Section
  SetShellVarContext current
  ${GetParameters} $Parameters
  StrCpy $ExtractOnly ""
  ClearErrors
  ${GetOptions} $Parameters "/extract-only=" $ExtractOnly
  StrCpy $INSTDIR "$LOCALAPPDATA\PLCSimulator\Standalone\@CACHE@"
  StrCmp $ExtractOnly "" check_cache
  StrCpy $INSTDIR $ExtractOnly
  Goto extract

check_cache:
  IfFileExists "$INSTDIR\payload.ready" 0 extract
  IfFileExists "$INSTDIR\PLCSimulator.exe" launch extract

extract:
  ClearErrors
  SetOutPath "$INSTDIR"
  File /r "@PAYLOAD@\*.*"
  IfErrors failed
  FileOpen $0 "$INSTDIR\payload.ready" w
  IfErrors failed
  FileWrite $0 "@HASH@"
  FileClose $0
  StrCmp $ExtractOnly "" launch
  SetErrorLevel 0
  Quit

launch:
  SetOutPath "$INSTDIR"
  ClearErrors
  ExecWait '"$INSTDIR\PLCSimulator.exe" $Parameters' $0
  IfErrors failed
  SetErrorLevel $0
  Quit

failed:
  MessageBox MB_OK|MB_ICONSTOP "PLCSimulator could not extract or start. Check free disk space and access to the cache directory."
  SetErrorLevel 1
SectionEnd
'@

# NSIS uses dollar signs for variable expansion, including inside quoted paths.
function ConvertTo-NsisLiteral([string]$Value) {
  return $Value.Replace('$', '$$').Replace('"', '$\"')
}
$nsis_script = $nsis_script.Replace('@OUTPUT@', (ConvertTo-NsisLiteral $output_path))
$nsis_script = $nsis_script.Replace('@ICON@', (ConvertTo-NsisLiteral (Join-Path $repo_root 'resources/icon/icon.ico')))
$nsis_script = $nsis_script.Replace('@PAYLOAD@', (ConvertTo-NsisLiteral $payload_dir))
$nsis_script = $nsis_script.Replace('@VERSION@', $Version)
$nsis_script = $nsis_script.Replace('@CACHE@', $cache_id).Replace('@HASH@', $payload_hash)
[IO.File]::WriteAllText($script_path, $nsis_script, [Text.UTF8Encoding]::new($true))
& $MakeNsis /V3 $script_path
if ($LASTEXITCODE -ne 0) {
  throw "NSIS failed with exit code $LASTEXITCODE."
}
Get-Item -LiteralPath $output_path | Select-Object FullName, Length
