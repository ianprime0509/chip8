configure_file(input : 'version.texi.in',
               output : 'version.texi',
               configuration : config)

custom_target('manual-info',
              output : 'chip8.info',
              input : 'chip8.texi',
              command : [texi2any, '--info', '@INPUT@',
                         '-I', meson.current_build_dir(),
                         '-o', '@OUTPUT@'])

custom_target('manual-html',
              output : 'html',
              input : 'chip8.texi',
              command : [texi2any, '--html', '@INPUT@',
                         '--css-include', 'style.css',
                         '-I', meson.current_build_dir(),
                         '-o', '@OUTPUT@'])
