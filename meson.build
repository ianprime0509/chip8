project('chip8', 'c',
        version : '0.1.0',
        license : 'MIT',
        default_options : ['warn_level=3',
                           'c_std=c99'])
add_global_arguments('-D_XOPEN_SOURCE=700', language : 'c')

config = configuration_data()
config.set('PROJECT_NAME', meson.project_name())
config.set('PROJECT_VERSION', meson.project_version())

incdir = include_directories('include')

if get_option('doxygen')
  subdir('docs')
endif

sdl = dependency('SDL2')

subdir('include')
subdir('man')
subdir('src')
subdir('test')
