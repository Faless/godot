_module_webxr_srcs = files([
    'register_types.cpp',
    'webxr_interface_js.cpp',
    'webxr_interface.cpp'
])

if PLATFORM == 'javascript'
    dir_base = meson.current_source_dir()
    DEPENDENCIES += declare_dependency(link_args: ['--js-library', join_paths(dir_base, 'native/library_godot_webxr.js')])
    # env.AddJSExterns(["native/webxr.externs.js"]) TODO externs, env variable, meson devs...
endif

lib_module_webxr = library('module_webxr', _module_webxr_srcs, dependencies: [DEP_GODOT_ALL])

DEP_MODULE_WEBXR = declare_dependency(link_with: lib_module_webxr)

MODULE_DEPENDENCIES += DEP_MODULE_WEBXR
