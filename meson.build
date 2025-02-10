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

cc = meson.get_compiler('c')

sources = []

subdir('src')

sdl3_dep = dependency('sdl3')

executable('frog2d',
    sources,
    win_subsystem: 'windows',
    include_directories : incdir,
    dependencies: sdl3_dep,
)
