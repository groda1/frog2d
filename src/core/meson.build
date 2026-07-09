
subdir('types')

core_sources += files(
    'memory_arena.c',
    'core_string.c',
    'file.c',
    'log.c'
)

if host_machine.system() == 'linux'
    core_sources += files('os_memory_linux.c')
else
    error('No os_memory backend configured for platform: ' + host_machine.system())
endif
