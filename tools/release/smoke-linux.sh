#!/usr/bin/env bash
# smoke-linux.sh - CI checks of the Linux package (docs/release-linux.md "How CI tests it"). Run on the GitHub runner
# after tools/release/build-linux.sh. It runs the package it is given, so do not point it at a package you do not
# trust. Never gets game data.
#
# usage: tools/release/smoke-linux.sh Homeworld-Infection-<version>-Linux-x86_64.tar.gz
#   1. the tar: one folder with the AppImage (executable), README-Linux.txt and licenses/
#   2. --appimage-extract, then ldd on usr/bin/hwi-engine and the bundled libraries: every library resolves, either
#      to usr/lib inside the AppImage or to a system library on the allow-list (glibc, libGL/GLX, libX11 and its xcb
#      chain); libSDL2 needs only glibc (its window-system and audio libraries are dlopened)
#   3. usr/bin/innoextract --version
#   4. the engine under xvfb-run (Mesa), no game data, SDL_AUDIODRIVER=dummy, HWI_DATASETUP_NONINTERACTIVE=1, with a
#      timeout: it must exit (not hang, not die from a signal). Twice: the binary with HW_Data = an empty folder, and
#      the AppImage itself (APPIMAGE_EXTRACT_AND_RUN=1: runtime + AppRun) with no HW_Data in a throwaway HOME
#   5. AppRun's menu entry in that HOME: written, valid (desktop-file-validate), icons installed, unchanged on a
#      second start, and following the AppImage to a new path (with a space and a $ in it)
# Needs: xvfb-run (xvfb, xauth), Mesa (libgl1-mesa-dri), desktop-file-validate; appstreamcli is optional.
set -euo pipefail
pkg=$(readlink -f "${1:?usage: smoke-linux.sh PACKAGE.tar.gz}")
t=$(mktemp -d); trap 'rm -rf -- "$t"' EXIT
fails=0
fail() { echo "SMOKE FAIL: $*"; fails=$((fails + 1)); }
ok() { echo "SMOKE ok:   $*"; }
summary() { [[ -z "${GITHUB_STEP_SUMMARY:-}" ]] || printf '%s\n' "$*" >> "$GITHUB_STEP_SUMMARY"; }
summary "## Linux package smoke test" "" "\`$(basename "$pkg")\` sha256 \`$(sha256sum "$pkg" | cut -d' ' -f1)\`" ""

echo "== 1. the tar"
mkdir -p "$t/x"; tar -xzf "$pkg" -C "$t/x"
tar -tvzf "$pkg"
img=$(find "$t/x" -maxdepth 2 -name '*.AppImage' | head -1)
[[ -n "$img" ]] || { echo "SMOKE FAIL: no AppImage in the tar"; exit 1; }
[[ -x "$img" ]] && ok "the AppImage is executable after extraction" || fail "the AppImage lost its executable bit"
[[ -f "$(dirname "$img")/README-Linux.txt" ]] && ok "README-Linux.txt" || fail "no README-Linux.txt beside the AppImage"
[[ -d "$(dirname "$img")/licenses" ]] && ok "licenses/: $(ls "$(dirname "$img")/licenses" | tr '\n' ' ')" || fail "no licenses/"

echo "== 2. the AppDir and its libraries"
mkdir -p "$t/e"; (cd "$t/e" && "$img" --appimage-extract >/dev/null)
r="$t/e/squashfs-root"
(cd "$r" && find . \( -type f -o -type l \) -printf '%p %s\n' | sort)
for f in AppRun homeworld-infection.desktop homeworld-infection.png usr/bin/hwi-engine usr/bin/innoextract \
	usr/lib/libSDL2-2.0.so.0 usr/share/metainfo/io.github.fivgo.HomeworldInfection.metainfo.xml \
	usr/share/doc/homeworld-infection/LICENSE-REULA.txt usr/share/doc/homeworld-infection/LICENSE-SDL2.txt \
	usr/share/doc/homeworld-infection/COPYING-FFmpeg-LGPLv2.1.txt usr/share/doc/homeworld-infection/SOURCE-FFmpeg.txt \
	usr/share/doc/homeworld-infection/LICENSE-innoextract.txt; do
	[[ -e "$r/$f" ]] || fail "missing $f"
done
allow='^(linux-vdso\.so\.1|ld-linux-x86-64\.so\.2|libc\.so\.6|libm\.so\.6|libdl\.so\.2|libpthread\.so\.0|librt\.so\.1|libGL\.so\.1|libGLX\.so\.0|libGLdispatch\.so\.0|libX11\.so\.6|libxcb\.so\.1|libXau\.so\.6|libXdmcp\.so\.6|libbsd\.so\.0|libmd\.so\.0)$'
summary "| file | library | resolves to | origin |" "|---|---|---|---|"
for f in "$r/usr/bin/hwi-engine" "$r"/usr/lib/*.so*; do
	rel=${f#"$r/"}
	echo "-- $rel: RUNPATH $(readelf -d "$f" | sed -n 's/.*(RUNPATH).*\[\(.*\)\]/\1/p'), NEEDED $(readelf -d "$f" | sed -n 's/.*(NEEDED).*\[\(.*\)\]/\1/p' | tr '\n' ' ')"
	while read -r lib arrow path _; do
		[[ -n "$lib" ]] || continue
		if [[ "$arrow" != "=>" ]]; then path=$lib; lib=$(basename "$lib"); fi   # ld-linux / vdso lines
		case "$path" in
			"not") fail "$rel: $lib not found"; origin="NOT FOUND" ;;
			"$r"/*) origin=bundled; path=${path#"$r/"} ;;
			*) if [[ "$lib" =~ $allow ]]; then origin=system; else origin="system, NOT ALLOWED"; fail "$rel needs $lib from the system ($path)"; fi ;;
		esac
		printf '   %-28s %-60s %s\n' "$lib" "$path" "$origin"
		summary "| $rel | $lib | $path | $origin |"
	done < <(env -u LD_LIBRARY_PATH ldd "$f" | sed 's/ (0x[0-9a-f]*)$//')
done
summary ""
[[ "$(readelf -d "$r/usr/bin/hwi-engine" | sed -n 's/.*(RUNPATH).*\[\(.*\)\]/\1/p')" == '$ORIGIN/../lib' ]] \
	&& ok "hwi-engine RUNPATH \$ORIGIN/../lib" || fail "hwi-engine RUNPATH is not \$ORIGIN/../lib"
sdl_needed=$(readelf -d "$r/usr/lib/libSDL2-2.0.so.0" | sed -n 's/.*(NEEDED).*\[\(.*\)\]/\1/p' | tr '\n' ' ')
if grep -qiE 'X11|xcb|wayland|decor|pulse|pipewire|asound|dbus|udev' <<< "$sdl_needed"; then
	fail "libSDL2 links a window-system or audio library directly: $sdl_needed"
else
	ok "libSDL2 needs only: $sdl_needed"
fi
desktop-file-validate "$r/homeworld-infection.desktop" && ok "the AppImage's desktop file validates" || fail "desktop-file-validate (AppDir)"
if command -v appstreamcli >/dev/null 2>&1; then
	if appstreamcli validate --no-net "$r/usr/share/metainfo/io.github.fivgo.HomeworldInfection.metainfo.xml"; then
		ok "appstreamcli validate"
	else
		echo "SMOKE note: appstreamcli findings above (informational, not a failure)"
	fi
fi

echo "== 3. innoextract"
file "$r/usr/bin/innoextract" || true
ldd "$r/usr/bin/innoextract" || true
if ie=$("$r/usr/bin/innoextract" --version 2>&1); then ok "innoextract --version: $(head -1 <<< "$ie")"; echo "$ie"
else fail "innoextract --version"; echo "$ie"; fi
summary "innoextract: \`$(head -1 <<< "$ie")\`" ""

echo "== 4. the engine without game data"
run_engine() {   # run_engine <label> <command...>: in $t/cwd under xvfb-run with a timeout; checks it exits
	local label=$1 rc; shift
	set +e
	(cd "$t/cwd" && exec timeout -k 15 120 xvfb-run -a -s "-screen 0 1280x1024x24" "$@") > "$t/$label.out" 2>&1
	rc=$?
	set -e
	echo "--- $label: exit $rc; last lines of its output:"; tail -n 25 "$t/$label.out" | sed 's/^/   | /'
	# 124/137: timeout's own codes; 129..192: killed by a signal (128 + 1..64). The engine's own error exit is
	# ERR_ErrorStart 0xfed5, seen as 213.
	if [[ $rc -eq 124 || $rc -eq 137 ]]; then fail "$label: did not exit within 120 s (hang)"
	elif [[ $rc -gt 128 && $rc -le 192 ]]; then fail "$label: died from signal $((rc - 128))"
	else ok "$label: exited by itself with code $rc"; fi
	summary "- $label: exit code $rc"
}
mkdir -p "$t/home1" "$t/empty" "$t/cwd"
export SDL_AUDIODRIVER=dummy HWI_DATASETUP_NONINTERACTIVE=1 LIBGL_ALWAYS_SOFTWARE=1
run_engine "hwi-engine-HW_Data-empty" env HOME="$t/home1" HW_Data="$t/empty" "$r/usr/bin/hwi-engine"

echo "== 5. the AppImage (runtime + AppRun) and its menu entry"
home="$t/home2"; mkdir -p "$home"
app="$home/.local/share/applications/homeworld-infection-appimage.desktop"
start_appimage() {   # start_appimage <label> <appimage>: the runtime extracts itself (no FUSE on the runner) and runs AppRun
	run_engine "$1" env -u HW_Data -u XDG_DATA_HOME HOME="$home" APPIMAGE_EXTRACT_AND_RUN=1 "$2"
}
start_appimage "appimage-first-start" "$img"
if [[ -f "$app" ]]; then
	ok "menu entry written"; cat "$app"
	desktop-file-validate "$app" && ok "the menu entry validates" || fail "desktop-file-validate (menu entry)"
	grep -qxF "Exec=\"$img\"" "$app" && ok "Exec is the AppImage's path" || fail "Exec is not \"$img\""
else
	fail "no menu entry at $app"
fi
n_icons=$(find "$home/.local/share/icons/hicolor" -name homeworld-infection-appimage.png 2>/dev/null | wc -l)
[[ $n_icons -eq 7 ]] && ok "7 icon sizes installed" || fail "$n_icons icon sizes installed (want 7)"
[[ -f "$home/.homeworld/hwi-engine.log" ]] && ok "AppRun wrote ~/.homeworld/hwi-engine.log" || fail "no ~/.homeworld/hwi-engine.log"
echo "--- hwi-engine.log:"; tail -n 25 "$home/.homeworld/hwi-engine.log" 2>/dev/null | sed 's/^/   | /' || true
before=$(stat -c %Y.%i "$app" 2>/dev/null || echo none)
sleep 1
start_appimage "appimage-second-start" "$img"
[[ "$(stat -c %Y.%i "$app" 2>/dev/null || echo none)" == "$before" ]] && ok "second start left the entry untouched" \
	|| fail "second start rewrote an unchanged entry"
[[ -f "$home/.homeworld/hwi-engine.log.old" ]] && ok "previous log kept as hwi-engine.log.old" || fail "no hwi-engine.log.old"
moved="$t/moved dir \$x"; mkdir -p "$moved"; cp "$img" "$moved/Homeworld Infection.AppImage"
start_appimage "appimage-moved" "$moved/Homeworld Infection.AppImage"
want_exec='Exec="'"$t"'/moved dir \\$x/Homeworld Infection.AppImage"'   # $ -> \$ (quoting) -> \\$ (string)
if grep -qxF "$want_exec" "$app"; then ok "the entry followed the moved AppImage (escaped: $(grep '^Exec=' "$app"))"
else fail "after the move: $(grep '^Exec=' "$app" || true), want $want_exec"; fi
desktop-file-validate "$app" && ok "the moved entry validates" || fail "desktop-file-validate (moved entry)"

echo
if [[ $fails -eq 0 ]]; then echo "smoke-linux.sh: all checks passed"; summary "**All checks passed.**"
else echo "smoke-linux.sh: $fails check(s) FAILED"; summary "**$fails check(s) failed.**"; exit 1; fi
