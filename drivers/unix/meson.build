if meson.get_compiler('cpp').has_header('mntent.h')
    CPP_ARGS += ['-DHAVE_MNTENT']
endif

SOURCES += files(run_command(globber, '*.cpp').stdout().split('\n'))
