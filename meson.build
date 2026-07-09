project('frog2d', 'c',
        default_options: [
            'buildtype=debugoptimized',
            'c_std=c23',
            'warning_level=everything',
            'werror=true'
        ]
)

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
