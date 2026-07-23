fs = import('fs')

resource_targets = []
foreach t : ['suzanne.obj', 'sphere.obj', 'test_model.frog']
    resource_targets += fs.copyfile(t)
endforeach
