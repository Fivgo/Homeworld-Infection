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
#     monochrome-btg, x86_64 converters) and the emscripten/darwin branches are not built
#
# This source is REULA-licensed (non-commercial): see src/COPYING.REULA.

project(
	'hwi-engine',
	'c',
	version: 'm2-bootstrap',
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
endif

c_base_args += ['-DHW_GAME_HOMEWORLD']

if get_option('debug') and get_option('debug_asserts')
	c_args = c_base_args + '-DHW_BUILD_FOR_DEBUGGING'
else
	c_args = c_base_args + '-DHW_BUILD_FOR_DISTRIBUTION'
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

subdir('src/tools/kas2c')
subdir('src')

executable(
	'hwi-engine',
	src,
	include_directories: inc_src,
	link_with: [lib_sdl],
	dependencies: base_deps,
	c_args: c_args,
	install: true,
)
