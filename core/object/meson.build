gdivirtuals_gen = custom_target('gdivirtuals.gen',
    output: 'gdvirtual.gen.inc',
    command: [find_program('make_virtuals.py'), '@OUTPUT@'])


SOURCES += files([
'callable_method_pointer.cpp',
'class_db.cpp',
'message_queue.cpp',
'method_bind.cpp',
'object.cpp',
'ref_counted.cpp',
'script_language.cpp',
'undo_redo.cpp',
])
