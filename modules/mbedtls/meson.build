_module_mbedtls_srcs = files(run_command(globber, '*.cpp').stdout().split('\n'))
lib_module_mbedtls = library('module_mbedtls',
    _module_mbedtls_srcs,
    _certs_target,
    dependencies: [DEP_GODOT_ALL, DEP_MBEDTLS]
)

DEP_MODULE_MBEDTLS = declare_dependency(link_with: lib_module_mbedtls)

MODULE_DEPENDENCIES += DEP_MODULE_MBEDTLS
