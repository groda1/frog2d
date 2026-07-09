project('frog2d', 'c',
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

sources += core_sources
sources += vulkan_sources
sources += engine_sources
sources += game_sources

subdir('shaders')

shader_depend = declare_dependency (sources: shader_targets)

executable('frog2d',
    sources,
    win_subsystem: 'windows',
    include_directories : incdir,
    dependencies: [
        dependency('sdl3'),
        dependency('vulkan'),
        shader_depend,
    ],
)

subdir('test')
