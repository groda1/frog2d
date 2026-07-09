# Compiles all shaders collected in shader_files (from src/*/shaders/).
# This directory only exists so the .spv outputs land in <builddir>/shaders/.
glsllang = find_program('glslangValidator')

shader_targets = []
foreach s : shader_files
  shader_targets += custom_target(
    'shader @0@'.format(s),
    command : [glsllang, '--target-env', 'vulkan1.4', '@INPUT@', '-o',  '@OUTPUT@'],
    input : s,
    output : '@PLAINNAME@.spv',
  )
endforeach
