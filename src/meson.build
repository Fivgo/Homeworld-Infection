# Same layout as Gardens of Kadesh 3af31fb src/meson.build.
subdir('Game')
# CAT (M2-R8 kasf library): HW1's compiled mission scripts (src/Missions, kas2c) are out of the build; the kasf
#      library has cataclysm.exe's signatures (src/Game/SinglePlayer.c)
# CAT (M2-R8 kas-runtime-mission01): Cataclysm's kas2c missions are built instead, generated from cataclysm.exe
#      (src/Missions/meson.build, tools/m2_kas2c_gen.py)
subdir('Missions')
subdir('Ships')
subdir('SDL')
subdir('ThirdParty')

src = [src_game, src_missions, src_ships, src_third_party]
inc_src = [inc_src_game, inc_src_missions, inc_src_sdl, inc_src_ships, inc_src_third_party]
