
subdir('types')

core_sources += files(
    'memory_arena.c',
    'core_string.c',
    'file.c',
    'log.c'
)

if host_machine.system() == 'linux'
    core_sources += files(
        'os_memory_linux.c',
        'os_path_linux.c',
        'os_time_linux.c',
    )
elif host_machine.system() == 'windows'
    core_sources += files(
        'os_memory_win32.c',
        'os_path_win32.c',
        'os_time_win32.c',
    )
else
    error('No os backend configured for platform: ' + host_machine.system())
endif
