if get_option('tools')

    # NOTE: do not use files() here
    _exporter_list = [
        'android',
        'iphone',
        'javascript',
        'linuxbsd',
        'osx',
        'uwp',
        'windows'
    ]

    _exporter_target = custom_target(
        'register_exporters',
        output: ['register_exporters.gen.cpp'],
        command: [SCRIPT_REGISTER_EXPORTERS, _exporter_list, '@OUTPUT@'],
        build_by_default: true
    )
    SOURCES += [_exporter_target]

    # need to built the sources
    SOURCES += files(run_command(globber, '../platform/*/export/*.cpp').stdout().split('\n'))
    ################################################################################
    # Doc generation
    ################################################################################

    # Currently, this will only build once. If you need to regenerate the doc files,
    # either delete the generated doc files, or reconfigure.
    # This is because we glob the files in the directories.

    # We can fix this by having doc_tool output a class_list.xml file (or the like)
    # that we can depend on, and auto detect doc changes and rebuild the docs.

    # generated version file
    _docs_target = custom_target(
        'docs',
        output: ['doc_data_class_path.gen.h', 'doc_data_compressed.gen.h'],
        input: [MODULES_DB_FILE],
        command: [SCRIPT_DOCS, '@INPUT@', meson.project_source_root(), '@OUTPUT@'],
        build_by_default: true
    )
    SOURCES += [_docs_target]


    # Translations also will run once

    _doc_translations_target = custom_target(
        'doc_translations',
        output: ['doc_translations.gen.h'],
        command: [SCRIPT_TRANSLATIONS, 'doc/translations', meson.project_source_root(), 'doc', '@OUTPUT@'],
        build_by_default: true
    )
    SOURCES += [_doc_translations_target]

    _editor_translations_target = custom_target(
        'editor_translations',
        output: ['editor_translations.gen.h'],
        command: [SCRIPT_TRANSLATIONS, 'editor/translations', meson.project_source_root(), 'editor', '@OUTPUT@'],
        build_by_default: true
    )
    SOURCES += [_editor_translations_target]

    # ...and so will fonts

    _fonts_target = custom_target(
        'fonts',
        output: ['builtin_fonts.gen.h'],
        command: [SCRIPT_FONTS, 'thirdparty/fonts', meson.project_source_root(), '@OUTPUT@'],
        build_by_default: true
    )
    SOURCES += [_fonts_target]

    # ...aaaaaaaaaaaaaaand editor icons
    _editor_icons_target = custom_target(
        'editor_icons',
        output: ['editor_icons.gen.h'],
        command: [SCRIPT_EDITOR_ICONS, 'editor/icons', meson.project_source_root(), '@OUTPUT@'],
        build_by_default: true
    )
    SOURCES += [_editor_icons_target]


    # Add our sources
    SOURCES += files(run_command(globber, '*.cpp').stdout().split('\n'))

    subdir('debugger')
    subdir('fileserver')
    subdir('import')
    subdir('plugins')

endif
