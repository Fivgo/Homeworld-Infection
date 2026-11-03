# smoke-test.ps1 - the Windows package's CI smoke test (.github/workflows/build-windows.yml, docs/release-windows.md).
# On a throwaway Windows runner (it installs into the runner user's profile; CI never has game data):
#   1. Setup.exe /S, then the installed files, the exe's version, both shortcuts and the Apps & features entry
#   2. hwi-engine.exe with SDL_VIDEODRIVER=dummy and no game data: every DLL must load (no 0xC0000135 and kin) and
#      it must exit by itself within the timeout
#   3. innoextract.exe --version
#   4. uninstall.exe /S: program files, shortcuts and entry gone; the settings and the game data kept
#   5. Setup.exe /S again, then uninstall.exe /S /PURGE: the settings and the game data gone too
# usage: smoke-test.ps1 -Setup <Homeworld-Infection-x.y.z-Windows-Setup.exe> [-Version x.y.z] [-TimeoutSec 60]
param(
	[Parameter(Mandatory = $true)][string]$Setup,
	[string]$Version = '1.1.0',
	[int]$TimeoutSec = 60
)
$ErrorActionPreference = 'Continue'
$script:failures = 0
function Check([bool]$ok, [string]$what) {
	$line = if ($ok) { "PASS  $what" } else { "FAIL  $what" }
	Write-Host $line
	if (-not $ok) { $script:failures++ }
	if ($env:GITHUB_STEP_SUMMARY) { Add-Content -LiteralPath $env:GITHUB_STEP_SUMMARY -Value "- $line" }
}
function Hex([int]$code) { '0x{0:X8}' -f $code }

# A missing DLL or a crash must end the child with its status code, not with a dialog nobody can click:
# SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX | SEM_NOOPENFILEERRORBOX, inherited by every child process.
Add-Type -Namespace Hwi -Name Native -MemberDefinition '[DllImport("kernel32.dll")] public static extern uint SetErrorMode(uint mode);'
[void][Hwi.Native]::SetErrorMode(0x8003)
New-Item -Path 'HKCU:\Software\Microsoft\Windows\Windows Error Reporting' -Force | Out-Null
Set-ItemProperty -Path 'HKCU:\Software\Microsoft\Windows\Windows Error Reporting' -Name DontShowUI -Value 1 -Type DWord

$app = 'Homeworld Infection'
$Setup = (Resolve-Path -LiteralPath $Setup).Path
$inst = Join-Path $env:LOCALAPPDATA "Programs\$app"
$exe = Join-Path $inst 'hwi-engine.exe'
$uninstKey = "HKCU:\Software\Microsoft\Windows\CurrentVersion\Uninstall\$app"
$startLnk = Join-Path ([Environment]::GetFolderPath('Programs')) "$app.lnk"
$deskLnk = Join-Path ([Environment]::GetFolderPath('Desktop')) "$app.lnk"
$settings = Join-Path $env:APPDATA 'Homeworld-Infection'
$data = Join-Path $env:LOCALAPPDATA 'Homeworld-Infection'
$tmp = if ($env:RUNNER_TEMP) { $env:RUNNER_TEMP } else { [IO.Path]::GetTempPath() }
Write-Host "setup: $Setup ($((Get-Item -LiteralPath $Setup).Length) bytes, sha256 $((Get-FileHash -LiteralPath $Setup -Algorithm SHA256).Hash))"

function Install-Package([string]$what) {
	$p = Start-Process -FilePath $Setup -ArgumentList '/S' -Wait -PassThru
	Check ($p.ExitCode -eq 0) "${what}: Setup.exe /S exits 0 (got $($p.ExitCode))"
}

function Uninstall-Package([string]$arguments, [string]$what) {
	$u = Join-Path $inst 'uninstall.exe'
	if (-not (Test-Path -LiteralPath $u)) { Check $false "${what}: $u exists"; return }
	# uninstall.exe copies itself to %TEMP% and runs that copy: wait for the copy too
	$p = Start-Process -FilePath $u -ArgumentList $arguments -Wait -PassThru
	Check ($p.ExitCode -eq 0) "${what}: uninstall.exe $arguments exits 0 (got $($p.ExitCode))"
	foreach ($k in @(Get-CimInstance Win32_Process -Filter "ParentProcessId=$($p.Id)")) {
		Write-Host "  waiting for the uninstaller's copy $($k.Name) (pid $($k.ProcessId))"
		Wait-Process -Id $k.ProcessId -Timeout 120 -ErrorAction SilentlyContinue
	}
	$deadline = (Get-Date).AddSeconds(60)
	while ((Test-Path -LiteralPath $uninstKey) -and (Get-Date) -lt $deadline) { Start-Sleep -Milliseconds 500 }
	Start-Sleep -Seconds 2
}

function Test-Installed([string]$what) {
	Write-Host "installed files:"
	Get-ChildItem -LiteralPath $inst -Recurse -File | ForEach-Object { Write-Host ("  {0} {1}" -f $_.FullName.Substring($inst.Length + 1), $_.Length) }
	foreach ($f in 'hwi-engine.exe', 'SDL2.dll', 'innoextract.exe', 'README.txt', 'uninstall.exe',
		'licenses\REULA.txt', 'licenses\SDL2.txt', 'licenses\FFmpeg\COPYING.LGPLv2.1.txt', 'licenses\FFmpeg\SOURCE.txt',
		'licenses\innoextract\innoextract.LICENSE.txt', 'licenses\innoextract\Boost.LICENSE_1_0.txt',
		'licenses\innoextract\liblzma.COPYING.txt', 'licenses\innoextract\libstdc++.COPYING3.txt',
		'licenses\innoextract\libstdc++.COPYING.RUNTIME.txt') {
		Check (Test-Path -LiteralPath (Join-Path $inst $f)) "${what}: $f installed"
	}
	foreach ($lib in 'avcodec', 'avformat', 'avutil', 'swscale') {
		Check (@(Get-ChildItem -LiteralPath $inst -Filter "$lib-*.dll").Count -eq 1) "${what}: $lib-*.dll installed"
	}
	if (Test-Path -LiteralPath $exe) {
		$vi = (Get-Item -LiteralPath $exe).VersionInfo
		Check ($vi.ProductVersion -eq $Version -and $vi.FileVersion -eq $Version) "${what}: hwi-engine.exe version $($vi.ProductVersion) / $($vi.FileVersion), '$($vi.ProductName)'"
	}
	$shell = New-Object -ComObject WScript.Shell
	foreach ($lnk in $startLnk, $deskLnk) {
		$there = Test-Path -LiteralPath $lnk
		Check $there "${what}: shortcut $lnk"
		if ($there) {
			$s = $shell.CreateShortcut($lnk)
			Check ($s.TargetPath -eq $exe) "${what}:   target $($s.TargetPath)"
			Check ($s.WorkingDirectory -eq $inst) "${what}:   starts in $($s.WorkingDirectory)"
			Check ($s.IconLocation -like '*\hwi-engine.exe,0') "${what}:   icon $($s.IconLocation)"
		}
	}
	$k = Get-ItemProperty -LiteralPath $uninstKey -ErrorAction SilentlyContinue
	Check ($null -ne $k) "${what}: Apps & features entry $uninstKey"
	if ($null -ne $k) {
		Check ($k.DisplayName -eq $app) "${what}:   DisplayName '$($k.DisplayName)'"
		Check ($k.DisplayVersion -eq $Version) "${what}:   DisplayVersion '$($k.DisplayVersion)'"
		Check ($k.DisplayIcon -eq "$exe,0") "${what}:   DisplayIcon '$($k.DisplayIcon)'"
		Check ([int]$k.EstimatedSize -gt 1024) "${what}:   EstimatedSize $($k.EstimatedSize) KB"
		Check ([string]$k.Publisher -ne '') "${what}:   Publisher '$($k.Publisher)'"
		Check ($k.UninstallString -eq "`"$inst\uninstall.exe`"") "${what}:   UninstallString $($k.UninstallString)"
	}
}

function Test-ProgramGone([string]$what) {
	# the uninstaller's copy deletes uninstall.exe and the folder after it exits (Windows marks them for deletion):
	# give that up to 30 s before judging (v1.1.0's tag run saw uninstall.exe still there 2 s after the purge)
	$deadline = (Get-Date).AddSeconds(30)
	while ((Test-Path -LiteralPath $inst) -and (Get-Date) -lt $deadline) { Start-Sleep -Milliseconds 500 }
	$left = @(Get-ChildItem -LiteralPath $inst -Recurse -Force -ErrorAction SilentlyContinue)
	foreach ($f in $left) { Write-Host "  left behind: $($f.FullName)" }
	Check (-not (Test-Path -LiteralPath $inst)) "${what}: install folder removed"
	Check (-not (Test-Path -LiteralPath $startLnk)) "${what}: Start menu shortcut removed"
	Check (-not (Test-Path -LiteralPath $deskLnk)) "${what}: Desktop shortcut removed"
	Check (-not (Test-Path -LiteralPath $uninstKey)) "${what}: Apps & features entry removed"
}

# 1. install
Check (-not (Test-Path -LiteralPath $inst)) "clean runner: no $inst before the test"
Install-Package 'install'
Test-Installed 'install'

# 2. the engine without game data, headless
$env:SDL_VIDEODRIVER = 'dummy'
$env:SDL_AUDIODRIVER = 'dummy'
$out = Join-Path $tmp 'hwi-engine-stdout.txt'
$err = Join-Path $tmp 'hwi-engine-stderr.txt'
$p = Start-Process -FilePath $exe -WorkingDirectory $inst -NoNewWindow -PassThru `
	-RedirectStandardOutput $out -RedirectStandardError $err
$null = $p.Handle   # keeps the exit code readable after the process ends
if ($p.WaitForExit($TimeoutSec * 1000)) {
	$code = $p.ExitCode
	# the loader's status codes (0xC0000135, 0xC0000139, 0xC000007B, 0xC0000142) as the signed exit codes
	$loadFailures = @{}
	$loadFailures[-1073741515] = 'STATUS_DLL_NOT_FOUND (a DLL is missing)'
	$loadFailures[-1073741511] = 'STATUS_ENTRYPOINT_NOT_FOUND (a DLL lacks a function)'
	$loadFailures[-1073741701] = 'STATUS_INVALID_IMAGE_FORMAT (a DLL of the wrong architecture)'
	$loadFailures[-1073741502] = 'STATUS_DLL_INIT_FAILED'
	$why = if ($loadFailures.ContainsKey($code)) { $loadFailures[$code] } else { 'not a DLL load failure' }
	Check (-not $loadFailures.ContainsKey($code)) "engine: every DLL loads; exit code $code ($(Hex $code)): $why"
	Check $true "engine: exits by itself within $TimeoutSec s, no game data, SDL_VIDEODRIVER=dummy"
} else {
	Stop-Process -Id $p.Id -Force -ErrorAction SilentlyContinue
	Check $false "engine: exits by itself within $TimeoutSec s (it was still running: killed)"
}
Write-Host '--- engine stdout (last 40 lines)'; Get-Content -LiteralPath $out -Tail 40 -ErrorAction SilentlyContinue
Write-Host '--- engine stderr (last 40 lines)'; Get-Content -LiteralPath $err -Tail 40 -ErrorAction SilentlyContinue
Write-Host '---'
Remove-Item Env:SDL_VIDEODRIVER, Env:SDL_AUDIODRIVER

# 3. innoextract beside the engine
$ieOut = & (Join-Path $inst 'innoextract.exe') --version 2>&1 | Out-String
$ieCode = $LASTEXITCODE
Write-Host $ieOut.TrimEnd()
Check ($ieCode -eq 0 -and $ieOut -match 'innoextract 1\.9') "innoextract.exe --version exits $ieCode and says 1.9"

# 4. uninstall, keeping the settings and the game data (stand-in files: CI has no game data)
New-Item -ItemType Directory -Force -Path $settings, (Join-Path $data 'GameData') | Out-Null
Set-Content -LiteralPath (Join-Path $settings 'smoke-marker.txt') -Value 'settings'
Set-Content -LiteralPath (Join-Path $data 'GameData\smoke-marker.txt') -Value 'data'
Uninstall-Package '/S' 'uninstall'
Test-ProgramGone 'uninstall'
Check (Test-Path -LiteralPath (Join-Path $settings 'smoke-marker.txt')) "uninstall: settings folder kept ($settings)"
Check (Test-Path -LiteralPath (Join-Path $data 'GameData\smoke-marker.txt')) "uninstall: game data kept ($data)"

# 5. install again, then uninstall with /PURGE
Install-Package 'reinstall'
Check (Test-Path -LiteralPath $exe) 'reinstall: hwi-engine.exe installed'
Uninstall-Package '/S /PURGE' 'purge'
Test-ProgramGone 'purge'
Check (-not (Test-Path -LiteralPath $settings)) "purge: settings folder removed ($settings)"
Check (-not (Test-Path -LiteralPath $data)) "purge: game data folder removed ($data)"

if ($script:failures -gt 0) { Write-Host "$($script:failures) check(s) FAILED"; exit 1 }
Write-Host 'all checks passed'
exit 0
