if meson.get_compiler('cpp').has_header('mntent.h')
    CPP_ARGS += ['-DHAVE_MNTENT']
endif

SOURCES += files(
    'dir_access_unix.cpp',
    'file_access_unix.cpp',
    'ip_unix.cpp',
    'net_socket_posix.cpp',
    'os_unix.cpp',
    'syslog_logger.cpp',
    'thread_posix.cpp',
)
