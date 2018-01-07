configure_file(input : 'version.texi.in',
               output : 'version.texi',
               configuration : config)

custom_target('info',
              output : 'chip8.info',
              input : 'chip8.texi',
              command : [texi2any, '--info', '@INPUT@',
                         '-I', meson.current_build_dir(),
                         '-o', '@OUTPUT@'])
