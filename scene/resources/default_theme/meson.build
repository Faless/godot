# The theme data will only be triggered when it does not exist.

# generated version file
_default_theme_target = custom_target(
    'default_theme',
    output: ['theme_data.gen.h'],
    command: [SCRIPT_THEME_DATA, 'scene/resources/default_theme', meson.project_source_root(), '@OUTPUT@'],
    build_by_default: true
)

generated_font = custom_target('default_font',
    input: '../../../thirdparty/fonts/OpenSans_SemiBold.ttf',
    output: 'default_font.gen.h',
    command: [find_program('default_theme_builders.py'), '@OUTPUT@', '@INPUT@'])

SOURCES += [_default_theme_target, generated_font]

SOURCES += files(run_command(globber, '*.cpp').stdout().split('\n'))
