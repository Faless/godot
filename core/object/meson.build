gdivirtuals_gen = custom_target('gdivirtuals.gen',
    output: 'gdvirtual.gen.inc',
    command: [find_program('make_virtuals.py'), '@OUTPUT@'])

SOURCES += files(run_command(globber).stdout().split('\n'))
