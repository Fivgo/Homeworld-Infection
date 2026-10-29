#!/usr/bin/env bash
# setup.sh - the one-time setup for playing Homeworld: Cataclysm on the Homeworld-Infection engine (Linux, x86-64).
#
# Usage:
#   ./setup.sh                     looks for the GOG installer (setup_homeworld_emergence_*.exe) in this folder, in
#                                  original/ and in ~/Downloads
#   ./setup.sh <installer.exe>     uses that GOG offline installer
#   ./setup.sh <game folder>       uses an installed copy of the game instead (the folder that holds cataclysm.big,
#                                  e.g. a GOG install on a Windows drive)
#   ./setup.sh --shortcut          only (re)installs the shortcut
#   ./setup.sh --remove-shortcut   removes the shortcut and its icon, nothing else
#
# The steps (each is skipped when it is already done, so running it again is safe):
#   1. the build tools and libraries: on Ubuntu, Pop!_OS, Debian and Mint they are installed with apt-get (which asks
#      for your password); on other systems the script lists what to install
#   2. the game files: unpacked from the installer (innoextract) or copied from the folder into original/game/
#   3. the data folder the engine reads (links into original/game/; tools/m2_harness.sh --setup)
#   4. the engine: build/release/hwi-engine (tools/build.sh --release)
#   5. the "Homeworld Infection" shortcut: the applications menu and the Desktop (it runs tools/launch.sh)
# Nothing is written outside this folder except the shortcut, its icon (~/.local/share) and step 1's packages.
set -euo pipefail
root=$(cd "$(dirname "$(readlink -f "$0")")" && pwd)
game="$root/original/game"
shareDir="${XDG_DATA_HOME:-$HOME/.local/share}"
appFile="$shareDir/applications/homeworld-infection.desktop"
iconBase="$shareDir/icons/hicolor"
desktopDir=$( (xdg-user-dir DESKTOP 2>/dev/null || true) | head -1 )
[[ -n "$desktopDir" && "$desktopDir" != "$HOME" ]] || desktopDir="$HOME/Desktop"
deskFile="$desktopDir/homeworld-infection.desktop"
log="$root/build/setup.log"

step() { printf '\n== %s\n' "$*"; }
fail() { printf '\nsetup.sh: %s\n' "$*" >&2; exit 1; }

remove_shortcut() {
	rm -f -- "$appFile" "$deskFile"
	find "$iconBase" -path '*/apps/homeworld-infection.png' -delete 2>/dev/null || true
	command -v update-desktop-database >/dev/null 2>&1 && update-desktop-database "$shareDir/applications" >/dev/null 2>&1 || true
	echo "The Homeworld Infection shortcut and its icon are removed."
}

install_shortcut() {
	step "5/5 The shortcut"
	local icon="applications-games" ico size
	ico=$(ls "$game"/goggame-*.ico 2>/dev/null | head -1 || true)
	if [[ -n "$ico" ]] && command -v python3 >/dev/null 2>&1; then
		# the GOG icon's largest picture (stored as PNG inside the .ico); made here, never part of the repository
		size=$(python3 -I - "$ico" "$root/build/homeworld-infection.png" <<'PY'
import struct, sys
data = open(sys.argv[1], "rb").read()
best = None
for i in range(struct.unpack("<HHH", data[:6])[2]):
    size, offset = struct.unpack("<II", data[6 + 16 * i + 8:6 + 16 * i + 16])
    png = data[offset:offset + size]
    if png[:8] == b"\x89PNG\r\n\x1a\n":
        width = struct.unpack(">I", png[16:20])[0]
        if best is None or width > best[0]:
            best = (width, png)
if best:
    open(sys.argv[2], "wb").write(best[1])
    print(best[0])
PY
) || size=""
		if [[ -n "$size" ]]; then
			mkdir -p "$iconBase/${size}x${size}/apps"
			install -m 644 "$root/build/homeworld-infection.png" "$iconBase/${size}x${size}/apps/homeworld-infection.png"
			icon="$iconBase/${size}x${size}/apps/homeworld-infection.png"
		fi
	fi
	mkdir -p "$(dirname "$appFile")"
	cat > "$appFile" <<DESKTOP
[Desktop Entry]
Type=Application
Version=1.0
Name=Homeworld Infection
GenericName=Homeworld: Cataclysm (recreation)
Comment=Play Homeworld: Cataclysm on the Homeworld-Infection engine (needs your GOG copy of Homeworld: Emergence)
Exec="$root/tools/launch.sh"
Icon=$icon
Terminal=false
Categories=Game;StrategyGame;
Keywords=Homeworld;Cataclysm;Emergence;
StartupNotify=true
StartupWMClass=homeworld-infection
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

case "${1:-}" in
	-h|--help) sed -n '2,20p' "$0" | sed 's/^# \{0,1\}//'; exit 0 ;;
	--remove-shortcut) remove_shortcut; exit 0 ;;
	--shortcut)
		[[ -e "$game/cataclysm.big" ]] || fail "the game files are not set up yet: run ./setup.sh first"
		install_shortcut; exit 0 ;;
esac

[[ "$(uname -s)" == Linux && "$(uname -m)" == x86_64 ]] || fail "this setup is for Linux on a 64-bit PC (x86-64)."
if [[ "$(git -C "$root" rev-parse --absolute-git-dir 2>/dev/null)" != "$(git -C "$root" rev-parse --path-format=absolute --git-common-dir 2>/dev/null)" ]]; then
	fail "this is a git worktree: run setup.sh in the main checkout."
fi
mkdir -p "$root/build"
chmod +x "$root"/tools/*.sh 2>/dev/null || true             # a ZIP download may lose the executable bits

# --- 1. tools and libraries ---------------------------------------------------------------------------------------
step "1/5 Build tools and libraries"
source_arg="${1:-}"
missing=()
want_cmd() { command -v "$1" >/dev/null 2>&1 || missing+=("$2"); }
want_pc() { pkg-config --exists "$1" 2>/dev/null || missing+=("$2"); }
want_cmd meson meson; want_cmd ninja ninja-build; want_cmd cc gcc; want_cmd pkg-config pkg-config
want_cmd flex flex; want_cmd bison bison
want_pc sdl2 libsdl2-dev; want_pc gl libgl-dev; want_pc x11 libx11-dev; want_pc libavcodec libavcodec-dev
want_pc libavformat libavformat-dev; want_pc libavutil libavutil-dev; want_pc libswscale libswscale-dev
if [[ ! -e "$game/cataclysm.big" && ! -d "$source_arg" ]]; then
	want_cmd innoextract innoextract; want_cmd file file; want_cmd python3 python3   # tools/extract_installer.sh
fi
if [[ ${#missing[@]} -gt 0 ]]; then
	echo "Needed: ${missing[*]}"
	if command -v apt-get >/dev/null 2>&1; then
		echo "Installing them with apt-get (sudo asks for your password) ..."
		sudo apt-get install -y "${missing[@]}" || fail "the packages could not be installed."
	else
		fail "please install the packages above with your system's package manager (the names are Debian/Ubuntu's;
  Fedora: meson ninja-build gcc pkgconf flex bison SDL2-devel mesa-libGL-devel libX11-devel ffmpeg-free-devel innoextract
  Arch:   meson ninja gcc pkgconf flex bison sdl2 mesa libx11 ffmpeg innoextract), then run ./setup.sh again."
	fi
fi
mesonv=$(meson --version 2>/dev/null || echo 0)
[[ "$(printf '%s\n1.1\n' "$mesonv" | sort -V | head -1)" == 1.1 ]] || fail "meson $mesonv is too old (1.1 or newer is needed)."
echo "ok"

# --- 2. the game files --------------------------------------------------------------------------------------------
step "2/5 The game files"
if [[ -e "$game/cataclysm.big" ]]; then
	echo "already in $game"
elif [[ -d "$source_arg" ]]; then
	src=$(cd "$source_arg" && pwd)
	[[ -e "$src/cataclysm.big" ]] || fail "$src has no cataclysm.big: give the folder Homeworld: Emergence is installed in."
	[[ -e "$src/update.big" ]] || echo "note: $src has no update.big (the GOG release has one); the game may not match."
	echo "copying the game files from $src ..."
	mkdir -p "$game"
	for e in cataclysm.big update.big Music4Cat.wxd SectSpeech.vce BeastSpeech.vce default_keys.cfg devstats.dat \
		credits feman Movies goggame-1766413869.ico; do
		[[ -e "$src/$e" ]] && cp -r -- "$src/$e" "$game/"
	done
	(cd "$game" && find . -type f ! -name 'MANIFEST.sizes' -printf '%s  ./%P\n' | LC_ALL=C sort -k2 > MANIFEST.sizes)
	echo "copied $(du -sh "$game" | cut -f1)"
else
	installer=""
	if [[ -n "$source_arg" ]]; then
		[[ -f "$source_arg" ]] || fail "$source_arg is neither a file nor a folder."
		installer=$(readlink -f -- "$source_arg")
	else
		installer=$(ls -t "$root"/setup_homeworld_emergence*.exe "$root"/original/setup_homeworld_emergence*.exe \
			"$HOME"/Downloads/setup_homeworld_emergence*.exe 2>/dev/null | head -1 || true)
	fi
	[[ -n "$installer" ]] || fail "no game files found. This needs your copy of Homeworld: Emergence from GOG:
  download the OFFLINE installer (GOG website -> Games -> Homeworld: Emergence -> 'Download offline backup game
  installers', a file named setup_homeworld_emergence_<version>.exe) into ~/Downloads or this folder and run
  ./setup.sh again; or run ./setup.sh <folder> with a folder the game is already installed in."
	echo "unpacking $installer ..."
	"$root/tools/extract_installer.sh" --installer "$installer" > "$log" 2>&1 || { tail -20 "$log"; fail "the installer could not be unpacked (the log: $log)."; }
	[[ -e "$game/cataclysm.big" ]] || { tail -20 "$log"; fail "the installer gave no cataclysm.big (the log: $log)."; }
	echo "unpacked $(du -sh "$game" | cut -f1)"
fi

# --- 3. the data folder -------------------------------------------------------------------------------------------
step "3/5 The data folder"
"$root/tools/m2_harness.sh" --setup > "$log" 2>&1 || { tail -20 "$log"; fail "the data folder could not be made (the log: $log)."; }
echo "ok: $root/original/scratch-m2/m2r2-harness/data"

# --- 4. the engine ------------------------------------------------------------------------------------------------
step "4/5 The engine"
if pgrep -f -- "$root/build/release/hwi-engine" >/dev/null 2>&1; then
	echo "the game is running: the build is left for the next start (the shortcut builds it then)"
else
	echo "building (about a minute the first time) ..."
	"$root/tools/build.sh" --release > "$log" 2>&1 || { tail -30 "$log"; fail "the build failed (the log: $log)."; }
	echo "ok: $root/build/release/hwi-engine"
fi

install_shortcut

cat <<DONE

All set. Start the game with the "Homeworld Infection" shortcut (in the applications menu and on the Desktop),
or run: $root/tools/launch.sh
The intro logos play first; any key skips the one playing. Settings, saved games and logs: $root/original/play-home
DONE
