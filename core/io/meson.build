# generated certs

_certs_target = custom_target(
    'certs',
    output: ['certs_compressed.gen.h'],
    input: [meson.project_source_root() + '/thirdparty/certs/ca-certificates.crt'],
    command: [SCRIPT_CERTS, '@INPUT@', '@OUTPUT@', get_option('system_certs_path'), get_option('builtin_certs') ? 'true' : 'false'],
    build_by_default: true
)

SOURCES += [_certs_target]

SOURCES += files(run_command(globber, '*.cpp').stdout().split('\n'))
