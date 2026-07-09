# Copies texture files into <builddir>/resources/textures/ so the binary can
# load them relative to SDL_GetBasePath, like the compiled shaders.
fs = import('fs')

resource_targets = []
foreach t : ['test.png', 'font.png', 'font2.png']
    resource_targets += fs.copyfile(t)
endforeach
