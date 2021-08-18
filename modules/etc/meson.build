_module_etc_srcs = files(run_command(globber, '*.cpp').stdout().split('\n'))
_lib_module_etc = library('module_etc', _module_etc_srcs,
    dependencies: [DEP_GODOT_ALL, DEP_ETC2COMP]
)

DEP_MODULE_ETC = declare_dependency(link_with: _lib_module_etc)

MODULE_DEPENDENCIES += DEP_MODULE_ETC
