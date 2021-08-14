DEPENDENCIES += [DEP_VULKAN, DEP_SPIRV_REFLECT]

SOURCES += files(
    'rendering_device_vulkan.cpp',
    'vulkan_context.cpp',
    '../../thirdparty/volk/volk.c',
)

if PLATFORM == 'android'
   CPP_ARGS += '-DVK_USE_PLATFORM_ANDROID_KHR'
elif PLATFORM == 'iphone'
    CPP_ARGS += '-DVK_USE_PLATFORM_IOS_MVK'
elif PLATFORM == 'linuxbsd'
    CPP_ARGS += '-DVK_USE_PLATFORM_XLIB_KHR'
elif PLATFORM == 'osx'
    CPP_ARGS += '-DVK_USE_PLATFORM_MACOS_MVK'
elif PLATFORM == 'windows'
    CPP_ARGS += '-DVK_USE_PLATFORM_WIN32_KHR'
endif

CPP_ARGS += ['-DUSE_VOLK', '-DVMA_STATIC_VULKAN_FUNCTIONS=1']
DEPENDENCIES += declare_dependency(include_directories: '../../thirdparty/volk')
