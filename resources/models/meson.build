fs = import('fs')

resource_targets = []
foreach t : ['suzanne.obj', 'sphere.obj', 'cube.frog', 'human.frog']
    resource_targets += fs.copyfile(t)
endforeach

# blender/*.blend -> *.frog, straight into this source directory.
# Not part of any build, only runs when asked for by name:
#   meson compile -C <builddir> export-models
blender = find_program('blender', required : false)
run_target('export-models',
    command : [
        find_program('../../tools/export_models.py'),
        blender.found() ? blender.full_path() : 'blender',
        meson.current_source_dir() / 'blender',
        meson.current_source_dir(),
    ],
)
