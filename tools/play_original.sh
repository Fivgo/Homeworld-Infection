#!/usr/bin/env bash
# play_original.sh - the untouched GOG game (Homeworld: Emergence 1.01, installer build 12948) under Wine, for comparing
# with the recreation. The "Homeworld Cataclysm (original GOG)" shortcut runs it.
#   tools/play_original.sh --setup             one time: an untouched copy of the game in original/gog/game (extracted
#                                              from the GOG installer and checked against its file list), its own Wine
#                                              prefix original/gog/prefix with the OpenGL renderer preset, and the shortcut
#   tools/play_original.sh                     start it, in a Wine virtual-desktop window the size of the game's video mode
#   tools/play_original.sh --size 2560x1440    store that video mode first, then start (the in-game Options menu works too)
#   tools/play_original.sh --remove-shortcut   remove the shortcut, nothing else
#   tools/play_original.sh -- /window          extra arguments go to cataclysm.exe (after GOG's own /noswddraw)
# This copy is separate from original/game (patched by CET, used by the checksum oracle) and from prefix/; nothing here
# touches those. No gamescope: a Wine virtual desktop, as the oracle recordings use (docs/CONSTRAINTS.md rule 1).
# Needs GE-Proton10-34 and umu-run (docs/RUNBOOK.md). Messages: original/gog/launch.log; each run: logs/run_game-*.log.
set -uo pipefail
root=$(cd "$(dirname "$(readlink -f "$0")")/.." && pwd)
title="Homeworld Cataclysm (original GOG)"

# original/ (game files, never in git) belongs to the MAIN checkout, also when this runs from a linked worktree
orig="$root/original"
common=$(git -C "$root" rev-parse --path-format=absolute --git-common-dir 2>/dev/null || true)
[[ -n "$common" && -d "${common%/.git}/original" ]] && orig="${common%/.git}/original"
game="$orig/gog/game"
pfx="$orig/gog/prefix"
launchlog="$orig/gog/launch.log"
proton_name="${HWC_PROTON:-GE-Proton10-34}"
steam_root="${HWC_STEAM_ROOT:-$HOME/.local/share/Steam}"
default_size=1920x1080
# The game keeps its renderer and video mode in the prefix registry (HKLM, 32-bit view).
cat_key='[Software\\Wow6432Node\\Sierra On-Line\\Cataclysm]'

shareDir="${XDG_DATA_HOME:-$HOME/.local/share}"
appFile="$shareDir/applications/homeworld-cataclysm-original.desktop"
desktopDir=$( (xdg-user-dir DESKTOP 2>/dev/null || true) | head -1 )
[[ -n "$desktopDir" && "$desktopDir" != "$HOME" ]] || desktopDir="$HOME/Desktop"
deskFile="$desktopDir/homeworld-cataclysm-original.desktop"

tell() {   # tell <error|info> <text>: a dialog if the desktop has one, else a notification, else stderr
	local kind=$1 text=$2
	if [[ -t 2 ]]; then echo -e "$title: $text" >&2; return; fi
	if command -v zenity >/dev/null 2>&1; then
		zenity "--$kind" --title="$title" --width=520 --no-wrap --text="$text" >/dev/null 2>&1 && return
	fi
	if command -v kdialog >/dev/null 2>&1; then
		if [[ $kind == error ]]; then kdialog --title "$title" --error "$text"; else kdialog --title "$title" --msgbox "$text"; fi
		return
	fi
	if command -v notify-send >/dev/null 2>&1; then
		notify-send -a "$title" "$title" "$text" && return
	fi
	echo -e "$title: $text" >&2
}
die() { echo "play_original.sh: $*" >&2; exit 1; }

key_value() {   # key_value <system.reg> <name>: a value of the game's registry key, as stored ("..." or dword:...)
	[[ -f "$1" ]] || return 0
	K="$cat_key" N="\"$2\"=" awk 'index($0, "[") == 1 { s = (index($0, ENVIRON["K"]) == 1); next }
		s && index($0, ENVIRON["N"]) == 1 { v = substr($0, length(ENVIRON["N"]) + 1) } END { if (v != "") print v }' "$1"
}
saved_size() {   # the video mode the game will use, WxH (empty if none is stored)
	local w h
	w=$(key_value "$pfx/system.reg" screenWidth); h=$(key_value "$pfx/system.reg" screenHeight)
	[[ $w == dword:* && $h == dword:* ]] && echo "$((16#${w#dword:}))x$((16#${h#dword:}))"
}
reg_import() {   # reg_import <WxH> [all]: store the video mode (and with "all" the OpenGL renderer preset)
	local w=${1%x*} h=${1#*x} regf="$pfx/drive_c/hwi-original-seed.reg" protondir crc i
	protondir="$steam_root/compatibilitytools.d/$proton_name"
	[[ -f "$protondir/proton" ]] || protondir="$steam_root/steamapps/common/$proton_name"
	[[ -f "$protondir/proton" ]] || die "Proton '$proton_name' not found (docs/RUNBOOK.md)"
	{
		printf 'Windows Registry Editor Version 5.00\r\n\r\n[HKEY_LOCAL_MACHINE\\Software\\Wow6432Node\\Sierra On-Line\\Cataclysm]\r\n'
		if [[ ${2:-} == all ]]; then
			# The stock game falls back to the software renderer when the saved device CRC differs from the one it
			# computes (main.c doInit, mainForceSoftware). The CRC covers the Wine/GPU device list, not the modes, so
			# the value the project's own prefix holds fits this one (same Proton, same machine).
			crc=$(key_value "${orig%/original}/prefix/system.reg" deviceCRC)
			[[ $crc == dword:* ]] && printf '"deviceCRC"=%s\r\n' "$crc"
			printf '"deviceIndex"=dword:ffffffff\r\n"deviceToSelect"=""\r\n"d3dToSelect"=""\r\n"glToSelect"="opengl32.dll"\r\n'
		fi
		printf '"screenWidth"=dword:%08x\r\n"screenHeight"=dword:%08x\r\n"screenDepth"=dword:00000020\r\n' "$w" "$h"
	} > "$regf"
	env WINEPREFIX="$pfx" GAMEID=umu-1766413869 STORE=gog PROTONPATH="$protondir" PROTON_VERB=waitforexitandrun \
		umu-run reg import 'C:\hwi-original-seed.reg' >/dev/null 2>&1
	rm -f "$regf"
	for ((i = 0; i < 20; i++)); do   # wineserver writes system.reg when it shuts down
		[[ $(saved_size) == "$1" ]] && return 0
		sleep 0.5
	done
	die "could not store the video mode $1 in $pfx"
}

remove_shortcut() {
	rm -f "$appFile" "$deskFile"
	command -v update-desktop-database >/dev/null 2>&1 && update-desktop-database "$shareDir/applications" >/dev/null 2>&1 || true
	echo "The \"$title\" shortcut is removed (the game copy in $orig/gog is left as it is)."
}
install_shortcut() {
	local icon="applications-games"
	# the icon setup.sh made for the recreation (the same GOG icon); a generic one otherwise
	for f in "$shareDir"/icons/hicolor/*/apps/homeworld-infection.png; do [[ -f $f ]] && icon=$f; done
	mkdir -p "$(dirname "$appFile")"
	cat > "$appFile" <<DESKTOP
[Desktop Entry]
Type=Application
Version=1.0
Name=$title
GenericName=Homeworld: Emergence under Wine
Comment=The untouched GOG game, to compare with Homeworld Infection
Exec="$root/tools/play_original.sh"
Icon=$icon
Terminal=false
Categories=Game;StrategyGame;
Keywords=Homeworld;Cataclysm;Emergence;GOG;original;
StartupNotify=true
StartupWMClass=steam_app_1766413869
DESKTOP
	chmod 755 "$appFile"
	command -v update-desktop-database >/dev/null 2>&1 && update-desktop-database "$shareDir/applications" >/dev/null 2>&1 || true
	echo "applications menu: $appFile"
	if [[ -d "$desktopDir" ]]; then
		install -m 755 "$appFile" "$deskFile"
		command -v gio >/dev/null 2>&1 && gio set "$deskFile" metadata::trusted true >/dev/null 2>&1 || true
		echo "Desktop:           $deskFile"
	fi
}

setup() {
	mkdir -p "$orig/gog"
	if [[ ! -f "$game/cataclysm.exe" ]]; then
		echo "== the untouched game copy: $game"
		HWC_ORIG_DIR="$orig" HWC_GAME_ROOT="$game" "$root/tools/extract_installer.sh" "$@" || die "extraction failed"
	fi
	echo "== checking the copy against the installer's file list"
	(cd "$game" && sha256sum -c --quiet MANIFEST.sha256) || die "$game differs from the installer's files (move it aside and run --setup again)"
	echo "   all $(wc -l < "$game/MANIFEST.sha256") files match"
	if [[ ! -f "$pfx/system.reg" ]]; then
		echo "== its Wine prefix: $pfx"
		"$root/tools/setup_prefix.sh" --prefix "$pfx" || die "the Wine prefix could not be made"
	fi
	if [[ $(key_value "$pfx/system.reg" glToSelect) != '"opengl32.dll"' ]]; then
		echo "== the OpenGL renderer preset and the ${default_size} video mode"
		reg_import "$default_size" all
		[[ $(key_value "$pfx/system.reg" deviceCRC) == dword:* ]] ||
			echo "   (no device CRC to copy: the first start uses the software renderer; pick OpenGL in Options > Video once)"
	fi
	echo "== the shortcut"
	install_shortcut
	echo "Ready: start it with the \"$title\" shortcut, or tools/play_original.sh"
}

size=""
case "${1:-}" in
	--setup) shift; setup "$@"; exit $? ;;
	--remove-shortcut) remove_shortcut; exit 0 ;;
	-h|--help) sed -n '2,13p' "$0" | sed 's/^# \{0,1\}//'; exit 0 ;;
	--size) size="${2:-}"; shift 2 || true
		[[ $size =~ ^[0-9]+x[0-9]+$ ]] || die "--size needs WxH, e.g. --size 2560x1440" ;;
esac
[[ ${1:-} == -- ]] && shift

if [[ ! -f "$game/cataclysm.exe" || ! -f "$pfx/system.reg" ]]; then
	tell error "The original game is not set up yet.\n\nOpen a terminal in\n    $root\nand run\n    tools/play_original.sh --setup"
	exit 1
fi
exec 9>"$orig/gog/.launch.lock"
if ! flock -n 9; then
	tell info "The original game is already running."
	exit 0
fi

[[ -n "$size" ]] && reg_import "$size"
size=$(saved_size); size=${size:-$default_size}
first=$(( $(wc -l < "$launchlog" 2>/dev/null || echo 0) + 1 ))
run() {   # run <label> [run_game.sh options]: one attempt; sets rc and secs
	local label=$1 t0; shift
	echo "=== $(date '+%F %T') $label: run_game.sh $*" >> "$launchlog"
	t0=$(date +%s)
	"$root/tools/run_game.sh" --original --game-dir "$game" --prefix "$pfx" --no-gamescope "$@" >> "$launchlog" 2>&1
	rc=$?; secs=$(( $(date +%s) - t0 ))
	echo "=== $(date '+%F %T') exit $rc after ${secs}s" >> "$launchlog"
}
run "virtual desktop $size" --virtual-desktop "$size" --size "$size" -- "$@"
# Wine's virtual desktop has died at start with an X11 BadWindow when launched from a script (docs/m2-r2-oracle.md);
# then the game gets a plain window instead (run_game.sh adds the game's own /window).
if (( secs < 20 )) && tail -n +"$first" "$launchlog" | grep -q 'BadWindow'; then
	run "plain window (the virtual desktop failed)" --size "$size" -- "$@"
fi
if [[ $rc -ne 0 ]]; then
	runlog=$(tail -n +"$first" "$launchlog" | sed -n 's/.*\[run_game\] log file: //p' | tail -1)
	tell error "The original game stopped with an error (exit code $rc).\n\nThe messages are in\n    ${runlog:-$launchlog}"
fi
exit "$rc"
