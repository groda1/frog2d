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


sources = []

subdir('src')

glsllang = find_program('glslangValidator')
shaders = [
    '2d_text_ssbo.frag',
    '2d_text_ssbo.vert',
]
shader_targets = []
foreach s : shaders
  shader_targets += custom_target(
    'shader @0@'.format(s),
    command : [glsllang, '-V', '@INPUT@', '-o',  '@OUTPUT@'],
    input : 'src/shaders/' + s,
    output : '@PLAINNAME@.spv',
  )
endforeach

shader_depend = declare_dependency (sources: shader_targets)

# build shaders
#foreach shader : shaders
#    run_command('glslangValidator', '-V', join_paths(meson.current_source_dir(), 'src', 'main', 'resource', 'shaders', shader), '-o', join_paths(meson.current_build_dir(), 'resources', 'shaders', shader + '.spv'), check: true)
#endforeach

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

