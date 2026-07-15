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
platform_sources = []
vulkan_sources = []
engine_sources = []
game_sources = []
shader_files = []
sources = []

subdir('src')

subdir('shaders')

subdir('resources')

shader_depend = declare_dependency (sources: shader_targets)
resource_depend = declare_dependency (sources: resource_targets)

executable('dcfs',
    sources,
    win_subsystem: 'windows',
    # main.c interfaces with the game only
    include_directories : [core_inc, platform_inc, game_inc],
    link_with : dcfs_libs,
    dependencies: platform_deps + [
        vulkan_dep,
        m_dep,
        shader_depend,
        resource_depend,
    ],
)

subdir('test')
