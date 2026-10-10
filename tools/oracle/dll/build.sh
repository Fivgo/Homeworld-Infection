#!/usr/bin/env bash
# build.sh - build the GOG-side oracle hook DLL and its loader (docs/m2-r2-oracle.md "dll"):
#   build/oracle-dll/hwi_oracle.dll         (i686, PE32 DLL, kernel32 only, no CRT)
#   build/oracle-dll/hwi_oracle_loader.exe  (i686, PE32 console-less exe, kernel32 only, no CRT)
# Toolchain (user decision 6 of 2026-10-10): clang-18 --target=i686-pc-windows-msvc + lld-link + llvm-dlltool-18,
# all from the distribution's LLVM 18; no Windows SDK, no mingw. The outputs are never run here: copy them next to
# cataclysm.exe (original/game/, gitignored) for a live session, which the user runs.
#
# Usage: tools/oracle/dll/build.sh [--clean]
set -euo pipefail

root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
src="$root/tools/oracle/dll"
out="$root/build/oracle-dll"
[[ "${1:-}" == "--clean" ]] && rm -rf -- "$out"
mkdir -p "$out"

CC=${CC:-clang-18}
LINK=${LINK:-lld-link}
DLLTOOL=${DLLTOOL:-llvm-dlltool-18}
for t in "$CC" "$LINK" "$DLLTOOL"; do
	command -v "$t" > /dev/null || { echo "build.sh: $t not found (decision 6: clang-18 + lld-link first; mingw-w64 is the fallback and needs a package install)" >&2; exit 1; }
done

# freestanding 32-bit Windows code: no CRT, no SSE (the detours copy floats as bits; the only x87 op is the fstp that
# rounds univGetChecksum's result), no stack probes beyond 4 KB (the code keeps big buffers static)
cflags=(--target=i686-pc-windows-msvc -ffreestanding -fno-builtin -nostdlib -nostdinc -isystem "$("$CC" -print-resource-dir)/include"
	-O2 -march=i686 -mno-sse -mno-mmx -fno-stack-protector -fno-exceptions -fno-unwind-tables -fno-asynchronous-unwind-tables
	-Wall -Wextra -Werror -Wno-unused-parameter -std=c11)

"$DLLTOOL" -m i386 -k -d "$src/kernel32.def" -l "$out/kernel32.lib"
"$CC" "${cflags[@]}" -c "$src/hwi_oracle.c" -o "$out/hwi_oracle.obj"
"$CC" "${cflags[@]}" -c "$src/hwi_oracle_loader.c" -o "$out/hwi_oracle_loader.obj"
"$LINK" /nologo /dll /machine:x86 /nodefaultlib /entry:DllMain /subsystem:windows /safeseh:no /Brepro \
	/out:"$out/hwi_oracle.dll" "$out/hwi_oracle.obj" "$out/kernel32.lib"
"$LINK" /nologo /machine:x86 /nodefaultlib /entry:mainCRTStartup /subsystem:windows /safeseh:no /Brepro \
	/out:"$out/hwi_oracle_loader.exe" "$out/hwi_oracle_loader.obj" "$out/kernel32.lib"
ls -l "$out/hwi_oracle.dll" "$out/hwi_oracle_loader.exe"
