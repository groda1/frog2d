
subdir('core')
subdir('vulkan')
subdir('engine')
subdir('game')

incdir = include_directories(
  'core',
  'core/types',
  'vulkan',
  'engine',
  'game',
  'third_party/xxHash',
  'third_party/HandmadeMath',
)

core_inc = include_directories(
  'core',
  'core/types',
  'third_party/xxHash',
  'third_party/HandmadeMath',
)
vulkan_inc = include_directories('vulkan')
engine_inc = include_directories('engine')
game_inc = include_directories('game')

sdl3_dep = dependency('sdl3')
vulkan_dep = dependency('vulkan')

core_lib = static_library('core', core_sources,
    include_directories : core_inc,
)

vulkan_lib = static_library('vulkan', vulkan_sources,
    include_directories : [core_inc, vulkan_inc, engine_inc],
    dependencies : [sdl3_dep, vulkan_dep],
)

engine_lib = static_library('engine', engine_sources,
    include_directories : [core_inc, engine_inc, vulkan_inc],
    dependencies : [sdl3_dep, vulkan_dep],
)

game_lib = static_library('game', game_sources,
    include_directories : [core_inc, engine_inc, game_inc],
    dependencies : [sdl3_dep, vulkan_dep],
)

dcfs_libs = [core_lib, vulkan_lib, engine_lib, game_lib]

sources += files('main.c')
