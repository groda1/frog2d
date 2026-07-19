fs = import('fs')

resource_targets = []
foreach t : ['suzanne.obj', 'sphere.obj']
    resource_targets += fs.copyfile(t)
endforeach
