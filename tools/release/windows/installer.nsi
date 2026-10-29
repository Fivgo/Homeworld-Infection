; installer.nsi - the Homeworld Infection installer for Windows (NSIS 3, Modern UI 2; docs/release-windows.md).
; tools/release/build-windows.sh compiles it:
;   makensis -DVERSION=1.0.0 -DSTAGE=<staged files> -DICON=<.ico> -DFILES_INSTALL=<.nsh> -DFILES_UNINSTALL=<.nsh>
;            -DOUTFILE=<Setup.exe> installer.nsi
; FILES_INSTALL / FILES_UNINSTALL are the File and Delete lists the script writes from the stage, so the uninstaller
; removes exactly what was installed (never a whole folder the user may have chosen for something else).
; Per-user: no administrator prompt, %LOCALAPPDATA%\Programs\Homeworld Infection, shortcuts in the user's Start menu
; and on the Desktop, the Apps & features entry under HKCU. Silent use: Setup.exe /S installs; uninstall.exe /S
; removes the program and keeps the settings, saves and unpacked game data; uninstall.exe /S /PURGE removes those too.

Unicode true
ManifestDPIAware true
RequestExecutionLevel user
SetCompressor /SOLID lzma
CRCCheck on

!include "MUI2.nsh"
!include "FileFunc.nsh"
!include "LogicLib.nsh"
!include "Sections.nsh"
!include "x64.nsh"

!define APPNAME "Homeworld Infection"
!define EXE "hwi-engine.exe"
!define PUBLISHER "Homeworld-Infection project"
!define UNINST_KEY "Software\Microsoft\Windows\CurrentVersion\Uninstall\Homeworld Infection"
; docs/release-1.0.md contract 4: the settings (options, keys, saves, screenshots) and the unpacked game data
!define SETTINGS_DIR "$APPDATA\Homeworld-Infection"
!define DATA_DIR "$LOCALAPPDATA\Homeworld-Infection"

Name "${APPNAME} ${VERSION}"
OutFile "${OUTFILE}"
InstallDir "$LOCALAPPDATA\Programs\${APPNAME}"
InstallDirRegKey HKCU "${UNINST_KEY}" "InstallLocation"
BrandingText "${APPNAME} ${VERSION}"
ShowInstDetails show
ShowUninstDetails show

!define MUI_ICON "${ICON}"
!define MUI_UNICON "${ICON}"
!define MUI_ABORTWARNING

; --- installer pages
!define MUI_WELCOMEPAGE_TEXT "Setup will install ${APPNAME} ${VERSION} for your Windows account. No administrator rights are needed.$\r$\n$\r$\n${APPNAME} is a recreation of the Homeworld: Cataclysm engine. The game's own files are not included: you need Homeworld: Emergence from GOG, installed, or its offline installer setup_homeworld_emergence_*.exe. On its first start the game finds them by itself or asks where they are.$\r$\n$\r$\n$_CLICK"
!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_LICENSE "${STAGE}/licenses/REULA.txt"
!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_INSTFILES
!define MUI_FINISHPAGE_RUN
!define MUI_FINISHPAGE_RUN_TEXT "Start ${APPNAME}"
!define MUI_FINISHPAGE_RUN_FUNCTION StartGame
!define MUI_FINISHPAGE_SHOWREADME "$INSTDIR\README.txt"
!define MUI_FINISHPAGE_SHOWREADME_TEXT "Show the README"
!define MUI_FINISHPAGE_SHOWREADME_NOTCHECKED
!insertmacro MUI_PAGE_FINISH

; --- uninstaller pages: the program files always go; the settings and the game data only when ticked
!insertmacro MUI_UNPAGE_CONFIRM
!define MUI_PAGE_CUSTOMFUNCTION_PRE un.ComponentsPre
!define MUI_COMPONENTSPAGE_TEXT_TOP "The program files are always removed. Tick anything else you want removed; what stays unticked is kept for a later install."
!define MUI_COMPONENTSPAGE_TEXT_COMPLIST "Remove:"
!insertmacro MUI_UNPAGE_COMPONENTS
!insertmacro MUI_UNPAGE_INSTFILES

!insertmacro MUI_LANGUAGE "English"

VIProductVersion "${VERSION}.0"
VIFileVersion "${VERSION}.0"
VIAddVersionKey /LANG=${LANG_ENGLISH} "ProductName" "${APPNAME}"
VIAddVersionKey /LANG=${LANG_ENGLISH} "ProductVersion" "${VERSION}"
VIAddVersionKey /LANG=${LANG_ENGLISH} "FileVersion" "${VERSION}"
VIAddVersionKey /LANG=${LANG_ENGLISH} "FileDescription" "${APPNAME} ${VERSION} Setup"
VIAddVersionKey /LANG=${LANG_ENGLISH} "CompanyName" "${PUBLISHER}"
VIAddVersionKey /LANG=${LANG_ENGLISH} "LegalCopyright" "Based on the Homeworld source code by Relic Entertainment, Relic EULA: non-commercial use only"

Function .onInit
	${IfNot} ${RunningX64}
		MessageBox MB_OK|MB_ICONSTOP "${APPNAME} needs 64-bit Windows." /SD IDOK
		Abort
	${EndIf}
FunctionEnd

Function StartGame
	SetOutPath "$INSTDIR"
	Exec '"$INSTDIR\${EXE}"'
FunctionEnd

Section "Install"
	SetShellVarContext current
	!include "${FILES_INSTALL}"
	WriteUninstaller "$INSTDIR\uninstall.exe"
	; the shortcuts start in the install folder (SetOutPath sets their working directory)
	SetOutPath "$INSTDIR"
	CreateShortcut "$SMPROGRAMS\${APPNAME}.lnk" "$INSTDIR\${EXE}" "" "$INSTDIR\${EXE}" 0 SW_SHOWNORMAL "" "Play ${APPNAME} ${VERSION}"
	CreateShortcut "$DESKTOP\${APPNAME}.lnk" "$INSTDIR\${EXE}" "" "$INSTDIR\${EXE}" 0 SW_SHOWNORMAL "" "Play ${APPNAME} ${VERSION}"
	WriteRegStr HKCU "${UNINST_KEY}" "DisplayName" "${APPNAME}"
	WriteRegStr HKCU "${UNINST_KEY}" "DisplayVersion" "${VERSION}"
	WriteRegStr HKCU "${UNINST_KEY}" "DisplayIcon" "$INSTDIR\${EXE},0"
	WriteRegStr HKCU "${UNINST_KEY}" "Publisher" "${PUBLISHER}"
	WriteRegStr HKCU "${UNINST_KEY}" "InstallLocation" "$INSTDIR"
	WriteRegStr HKCU "${UNINST_KEY}" "UninstallString" '"$INSTDIR\uninstall.exe"'
	WriteRegStr HKCU "${UNINST_KEY}" "QuietUninstallString" '"$INSTDIR\uninstall.exe" /S'
	WriteRegDWORD HKCU "${UNINST_KEY}" "NoModify" 1
	WriteRegDWORD HKCU "${UNINST_KEY}" "NoRepair" 1
	${GetSize} "$INSTDIR" "/S=0K" $0 $1 $2
	IntFmt $0 "0x%08X" $0
	WriteRegDWORD HKCU "${UNINST_KEY}" "EstimatedSize" "$0"
SectionEnd

Section "un.Program files" SecUnProgram
	SectionIn RO
	SetShellVarContext current
	Delete "$SMPROGRAMS\${APPNAME}.lnk"
	Delete "$DESKTOP\${APPNAME}.lnk"
	!include "${FILES_UNINSTALL}"
	Delete "$INSTDIR\uninstall.exe"
	RMDir "$INSTDIR"
	DeleteRegKey HKCU "${UNINST_KEY}"
SectionEnd

Section /o "un.Settings and saved games" SecUnSettings
	RMDir /r "${SETTINGS_DIR}"
SectionEnd

Section /o "un.Unpacked game data" SecUnData
	RMDir /r "${DATA_DIR}"
SectionEnd

!insertmacro MUI_UNFUNCTION_DESCRIPTION_BEGIN
	!insertmacro MUI_DESCRIPTION_TEXT ${SecUnProgram} "The game program in $INSTDIR, its shortcuts and its Apps & features entry."
	!insertmacro MUI_DESCRIPTION_TEXT ${SecUnSettings} "Your options, key bindings, saved games and screenshots in ${SETTINGS_DIR}."
	!insertmacro MUI_DESCRIPTION_TEXT ${SecUnData} "The game data unpacked from the GOG offline installer, in ${DATA_DIR}. A GOG installation is never changed."
!insertmacro MUI_UNFUNCTION_DESCRIPTION_END

Function un.onInit
	SetShellVarContext current
	${un.GetParameters} $R0
	ClearErrors
	${un.GetOptions} $R0 "/PURGE" $R1
	${IfNot} ${Errors}
		!insertmacro SelectSection ${SecUnSettings}
		!insertmacro SelectSection ${SecUnData}
	${EndIf}
	; nothing there: no choice to offer
	${IfNot} ${FileExists} "${SETTINGS_DIR}\*.*"
		SectionSetText ${SecUnSettings} ""
	${EndIf}
	${IfNot} ${FileExists} "${DATA_DIR}\*.*"
		SectionSetText ${SecUnData} ""
	${EndIf}
FunctionEnd

Function un.ComponentsPre
	${IfNot} ${FileExists} "${SETTINGS_DIR}\*.*"
	${AndIfNot} ${FileExists} "${DATA_DIR}\*.*"
		Abort
	${EndIf}
FunctionEnd
