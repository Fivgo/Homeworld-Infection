# Homeworld-Infection: hwi-engine
#
# M2-R1 bootstrap: builds the Homeworld 1 engine (Relic source via Gardens of
# Kadesh 3af31fb, see src/PROVENANCE.md) with no Cataclysm changes yet.
# Adapted from Gardens of Kadesh's meson.build (reference/gardens-of-kadesh,
# commit 3af31fb). Differences from GoK, all build-system only:
#   - project/executable name hwi-engine; root is the repository, sources under src/
#   - -Werror comes from meson's own `werror` option (default true, as GoK)
#     instead of a hard-coded flag, so `-Dwerror=false` can count warnings
#   - the -Wno-* list is filtered through get_supported_arguments(), so a clang
#     build does not die on GCC-only flags (docs/hw1-port-lessons.md B4, C1.14);
#     the clang-only suppressions GoK applies only to emscripten are added for clang
#   - kas2c lives in src/tools/kas2c; GoK's optional tools (polycount, biggie,
#     monochrome-btg, x86_64 converters) and the emscripten branch are not built; the darwin branch is HWI's own
#     (HWI-FIX(macos), docs/release-macos.md)
#
# This source is REULA-licensed (non-commercial): see src/COPYING.REULA.

project(
	'hwi-engine',
	'c',
	version: '1.0.0',
	default_options: [
		'warning_level=3',
		'c_std=gnu11',
		'b_sanitize=address,undefined',
		'werror=true',
	],
	meson_version: '>=1.1.0',
)

cc = meson.get_compiler('c')

build_version = get_option('build_version')
if build_version == ''
	git = find_program('git', required: false)
	if git.found()
		git_describe = run_command(
			git,
			'-C', meson.project_source_root(),
			'describe',
			'--always',
			'--tags',
			'--dirty=+',
			check: false,
		)
		if git_describe.returncode() == 0
			build_version = git_describe.stdout().strip()
		endif
	endif
endif
if build_version == ''
	build_version = meson.project_version()
endif
message('Build version: ' + build_version)

if get_option('b_sanitize') != 'none'
	warning('Building with memory sanitizers on. tools/build.sh --release turns them off.')
endif

dep_sdl = dependency('sdl2')

base_deps = [
	dependency('gl'),
	dep_sdl,
	cc.find_library('m', required: false),
]

c_base_args = [
	'-DHAVE_CONFIG_H',
	'-D_REENTRANT',
]

# Determinism flags (M2-R2, user decision 2 of 2026-10-10; docs/m2-r2-oracle.md "flags"), pinned for every build
# type before any checksum comparison: no type-based alias analysis, no fused multiply-add contraction (the exe
# never fuses a*b+c), no fast-math. The optimisation level of the release build used for oracle runs, -O2, comes
# from tools/build.sh --release. Anything that re-enables fast-math through -Dc_args is refused.
determinism_args = [
	'-fno-strict-aliasing',
	'-ffp-contract=off',    # HWI-FIX(macos): also overrides clang's default (on) for arm64, which would fuse a*b+c
	'-fno-fast-math',
]
foreach a : get_option('c_args')
	if a.contains('fast-math') or a == '-Ofast' or a.contains('fp-contract=fast') or a.contains('unsafe-math')
		error('c_args ' + a + ' breaks the pinned floating-point flags (docs/m2-r2-oracle.md "flags")')
	endif
endforeach
c_base_args += determinism_args

# Existing issues in the legacy codebase (GoK's list, verbatim order).
warning_suppressions = [
	'-Wno-address',
	'-Wno-cast-function-type',
	'-Wno-discarded-qualifiers',
	'-Wno-enum-conversion',
	'-Wno-empty-body',
	'-Wno-format-security',
	'-Wno-format-zero-length',
	'-Wno-format',
	'-Wno-implicit-fallthrough',
	'-Wno-implicit-function-declaration',
	'-Wno-int-conversion',
	'-Wno-int-in-bool-context',
	'-Wno-int-to-pointer-cast',
	'-Wno-logical-not-parentheses',
	'-Wno-maybe-uninitialized',
	'-Wno-missing-field-initializers',
	'-Wno-misleading-indentation',
	'-Wno-multistatement-macros',
	'-Wno-parentheses',
	'-Wno-pedantic',
	'-Wno-pointer-arith',
	'-Wno-pointer-to-int-cast',
	'-Wno-restrict',
	'-Wno-sign-compare',
	'-Wno-stringop-overflow',
	'-Wno-stringop-overread',
	'-Wno-type-limits',
	'-Wno-unused-but-set-parameter',
	'-Wno-unused-but-set-variable',
	'-Wno-unused-function',
	'-Wno-unused-parameter',
	'-Wno-unused-value',
	'-Wno-unused-variable',
	# GoK: Windows mingw build issues
	'-Wno-array-bounds',
	'-Wno-deprecated-declarations',
	'-Wno-stringop-truncation',
	# GoK: Linux gcc 13.2.0
	'-Wno-unused-result',
]
if cc.get_id() == 'clang'
	# clang only (GoK applies these to emscripten; docs/hw1-port-lessons.md B4 try 3).
	# Kept out of GCC builds: -Wno-uninitialized and -Wno-pointer-sign are valid
	# GCC flags too and would hide warnings GoK's GCC build still reports.
	warning_suppressions += [
		'-Wno-constant-logical-operand',
		'-Wno-deprecated-non-prototype',
		'-Wno-pointer-sign',
		'-Wno-invalid-source-encoding',
		'-Wno-uninitialized',
		'-Wno-sometimes-uninitialized',
		'-Wno-non-literal-null-conversion',
		# HWI-FIX(win64): clang's name for GCC's -Wno-discarded-qualifiers (in the list above); the Windows build is
		# llvm-mingw clang, and the generated missions pass const strings to char * parameters (237 sites)
		'-Wno-incompatible-pointer-types-discards-qualifiers',
	]
endif
if get_option('legacy_warnings')
	c_base_args += cc.get_supported_arguments(warning_suppressions)
endif

if host_machine.cpu_family() != 'x86'
	c_base_args += ['-DGENERIC_ETGCALLFUNCTION']
endif

if host_machine.cpu_family() == 'x86_64'
	c_base_args += ['-DHW_PTR_64', '-DHW_PTR_64_FIX_ME']
elif host_machine.cpu_family() == 'x86'
	c_base_args += ['-malign-double', '-D_X86', '-msse']
elif host_machine.cpu_family() == 'aarch64'
	c_base_args += ['-DARM', '-DHW_PTR_64', '-DHW_PTR_64_FIX_ME']
endif

if host_machine.system() == 'linux'
	base_deps += [dependency('x11')]
	c_base_args += ['-D_LINUX_FIX_ME', '-D_GNU_SOURCE']
elif host_machine.system() == 'windows'
	c_base_args += ['-D_LINUX_FIX_ME', '-D_GNU_SOURCE', '-D_WIN32', '-DWIN32', '-DWINDOWS']
elif host_machine.system() == 'darwin'
	# HWI-FIX(macos): the Linux code paths (no X11; dependency('gl') is the OpenGL framework). Not GoK's _MACOSX /
	#      _MACOSX_FIX_* (HomeworldSDL's Xcode build): they turn off movies, sound and LAN. __APPLE__ marks what differs.
	c_base_args += ['-D_LINUX_FIX_ME', '-D_GNU_SOURCE', '-DGL_SILENCE_DEPRECATION']
endif

c_base_args += ['-DHW_GAME_HOMEWORLD']

if get_option('debug') and get_option('debug_asserts')
	hw_build_define = '-DHW_BUILD_FOR_DEBUGGING'
else
	hw_build_define = '-DHW_BUILD_FOR_DISTRIBUTION'
endif
c_args = c_base_args + hw_build_define

# Checksum oracle (docs/m2-r2-oracle.md): src/Game/OracleDump.c writes the .hwo stream; tools/oracle/hwo_format.h is
# the record layout it shares with the GOG-side DLL; hwi_oracle_build.h (generated) identifies the build in the
# stream header: the commit and an FNV-1a-32 of the flags that decide the floating-point results.
inc_oracle = include_directories('.', 'tools/oracle')
if get_option('oracle')
	c_args += ['-DHWI_ORACLE=1']
	git_hash = '0'
	git_prog = find_program('git', required: false)
	if git_prog.found()
		git_rev = run_command(git_prog, '-C', meson.project_source_root(), 'rev-parse', '--short=8', 'HEAD', check: false)
		if git_rev.returncode() == 0
			git_hash = git_rev.stdout().strip()
		endif
	endif
	oracle_flags = ' '.join([
		cc.get_id(), cc.version(), host_machine.cpu_family(),
		'optimization=' + get_option('optimization'),
		'debug=' + get_option('debug').to_string(),
		'sanitize=' + get_option('b_sanitize'),
	] + determinism_args + get_option('c_args') + [hw_build_define])
	oracle_conf = configuration_data()
	oracle_conf.set('HWI_GIT_HASH', '0x' + git_hash + 'u')
	oracle_conf.set_quoted('HWI_BUILD_FLAGS', oracle_flags)
	configure_file(output: 'hwi_oracle_build.h', configuration: oracle_conf)
	message('Oracle build identity: ' + git_hash + ' / ' + oracle_flags)
	if not get_option('debug') and get_option('optimization') != '2'
		warning('oracle reference streams are taken from the -O2 release build: use tools/build.sh --release')
	endif
else
	c_args += ['-DHWI_ORACLE=0']
endif

if get_option('movies')
	base_deps += [
		dependency('libavutil'),
		dependency('libswscale'),
		dependency('libavcodec'),
		dependency('libavformat'),
	]
	c_args += ['-DHW_ENABLE_MOVIES']
endif

# HWI-FIX(win64): kas2c is a build-machine tool (native: true) that links the HOST's SDL and c_base_args; no target
# uses it since M2-R8 (src/Missions is generated by tools/m2_kas2c_gen.py), so a cross build leaves it out.
if not meson.is_cross_build()
	subdir('src/tools/kas2c')
endif
subdir('src')

executable(
	'hwi-engine',
	src,
	include_directories: [inc_src, inc_oracle],
	link_with: [lib_sdl],
	dependencies: base_deps,
	c_args: c_args,
	install: true,
)
