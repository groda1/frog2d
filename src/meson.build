
subdir('core')
subdir('platform')
subdir('vulkan')
subdir('engine')
subdir('game')

core_inc = include_directories(
  'core',
  'core/types',
  'third_party/xxHash',
  'third_party/HandmadeMath',
)
platform_inc = include_directories('platform')
vulkan_inc = include_directories('vulkan')
engine_inc = include_directories('engine')
game_inc = include_directories('game')
stb_inc = include_directories('third_party/stb')

vulkan_dep = dependency('vulkan')
m_dep = meson.get_compiler('c').find_library('m', required : false)

if host_machine.system() == 'windows'
    platform_deps = [meson.get_compiler('c').find_library('user32')]
else
    platform_deps = [dependency('sdl3')]
endif

core_lib = static_library('core', core_sources,
    include_directories : core_inc,
)

platform_lib = static_library('platform', platform_sources,
    include_directories : [core_inc, platform_inc],
    dependencies : platform_deps + [vulkan_dep],
)

vulkan_lib = static_library('vulkan', vulkan_sources,
    include_directories : [core_inc, platform_inc, vulkan_inc, engine_inc],
    dependencies : [vulkan_dep],
)

engine_lib = static_library('engine', engine_sources,
    include_directories : [core_inc, platform_inc, engine_inc, vulkan_inc, stb_inc],
    dependencies : [vulkan_dep],
)

# no vulkan_dep: the game never sees vulkan types or headers
game_lib = static_library('game', game_sources,
    include_directories : [core_inc, platform_inc, engine_inc, game_inc],
    dependencies : [m_dep],
)

dcfs_libs = [core_lib, platform_lib, vulkan_lib, engine_lib, game_lib]

sources += files('main.c')
