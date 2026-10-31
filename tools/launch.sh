#!/usr/bin/env bash
# launch.sh - what the "Homeworld Infection" shortcut runs (./setup.sh installs the shortcut in the applications menu
# and on the Desktop). It starts the game through tools/play.sh with its defaults, without a terminal:
#   - the engine is built first when it is missing or older than the source (a notification says so; about a minute);
#   - a second click while the game runs only says that it is running;
#   - if the game cannot start, or stops with an error, a dialog says why and where the log is.
# The settings and saved games live in original/play-home (see tools/play.sh); the files you edit or look at, Assets/
# (textures, models, menu pictures) and Screenshots/, are at the top of the project folder; each session's log is
# original/play-home/logs/play-<date>-<time>.log, and this launcher's own messages go to original/play-home/logs/launch.log.
# Any arguments are passed to tools/play.sh (e.g. tools/launch.sh -- /noInterp).
set -uo pipefail
root=$(cd "$(dirname "$(readlink -f "$0")")/.." && pwd)
title="Homeworld Infection"

# the play home lives under the MAIN checkout's original/ (as tools/play.sh resolves it)
orig="$root/original"
if [[ ! -e "$orig/scratch-m2/m2r2-harness/data/cataclysm.big" ]]; then
	common=$(git -C "$root" rev-parse --path-format=absolute --git-common-dir 2>/dev/null || true)
	[[ -n "$common" && -e "${common%/.git}/original/scratch-m2/m2r2-harness/data/cataclysm.big" ]] && orig="${common%/.git}/original"
fi
home="$orig/play-home"
mkdir -p "$home/logs" 2>/dev/null || true
launchlog="$home/logs/launch.log"

tell() {   # tell <error|info> <text>: a dialog if the desktop has one, else a notification, else stderr
	local kind=$1 text=$2
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
	echo "$title: $text" >&2
}

if [[ ! -e "$orig/scratch-m2/m2r2-harness/data/cataclysm.big" ]]; then
	tell error "The game is not set up yet.\n\nOpen a terminal in\n    $root\nand run\n    bash setup.sh\n(it needs your GOG copy of Homeworld: Emergence; see README.md)."
	exit 1
fi

# one game at a time: the lock is held for as long as this launcher (and so the game) runs
exec 9>"$home/.launch.lock"
if ! flock -n 9; then
	tell info "Homeworld Infection is already running."
	exit 0
fi

# the window's class names the shortcut, so the dock and the task switcher show its icon
export SDL_VIDEO_X11_WMCLASS=homeworld-infection
export HWI_LAUNCHER=1                       # tools/play.sh: a notification before a build
first=$(( $(wc -l < "$launchlog" 2>/dev/null || echo 0) + 1 ))   # this launch's lines start here
{
	echo "=== $(date '+%F %T') launch: tools/play.sh $*"
} >> "$launchlog"
"$root/tools/play.sh" "$@" > /dev/null 2>> "$launchlog"
rc=$?
echo "=== $(date '+%F %T') exit $rc" >> "$launchlog"
if [[ $rc -ne 0 ]]; then
	this=$(tail -n +"$first" "$launchlog")
	playlog=$(sed -n 's/^play.sh: engine exit [0-9-]* (log \(.*\))$/\1/p' <<< "$this" | tail -1)
	if grep -q '^play.sh: building' <<< "$this" && [[ -z "$playlog" ]]; then
		tell error "The game could not be built (exit code $rc).\n\nThe build's messages are in\n    $launchlog"
	elif [[ -n "$playlog" ]]; then
		tell error "Homeworld Infection stopped with an error (exit code $rc).\n\nThe session's log is\n    $playlog"
	else
		tell error "Homeworld Infection could not start (exit code $rc).\n\nThe messages are in\n    $launchlog"
	fi
fi
exit "$rc"
