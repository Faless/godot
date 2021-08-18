_module_fbx_srcs = files(run_command(globber, '*.cpp').stdout().split('\n'))
_module_fbx_srcs += files(run_command(globber, 'data/*.cpp').stdout().split('\n'))
_module_fbx_srcs += files(run_command(globber, 'fbx_parser/*.cpp').stdout().split('\n'))
_module_fbx_srcs += files(run_command(globber, 'tools/*.cpp').stdout().split('\n'))

# add our current directory into the include path
_module_fbx_incdirs = include_directories('.')

lib_module_fbx = library('module_fbx',
                         _module_fbx_srcs,
                         gdivirtuals_gen,
                         include_directories: _module_fbx_incdirs,
                         dependencies: [DEP_GODOT_ALL, DEP_ZLIB])

DEP_MODULE_FBX = declare_dependency(link_with: lib_module_fbx)

MODULE_DEPENDENCIES += DEP_MODULE_FBX
