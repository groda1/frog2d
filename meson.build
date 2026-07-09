project('dcfs', 'c',
        default_options: [
            'buildtype=debugoptimized',
            'optimization=3',
            'c_std=c23',
            'warning_level=3',
            'werror=true'
        ]
)

if get_option('buildtype').startswith('debug' )
    add_project_arguments('-DDEBUG_BUILD', language: 'c')
endif


core_sources = []
vulkan_sources = []
engine_sources = []
game_sources = []
shader_files = []
sources = []

subdir('src')

subdir('shaders')

shader_depend = declare_dependency (sources: shader_targets)

executable('dcfs',
    sources,
    win_subsystem: 'windows',
    # main.c interfaces with the game only
    include_directories : [core_inc, game_inc],
    link_with : dcfs_libs,
    dependencies: [
        sdl3_dep,
        vulkan_dep,
        shader_depend,
    ],
)

subdir('test')
