doxygen = find_program('doxygen')
config.set('DOXYGEN_INPUTS', join_paths(meson.source_root(), 'include'))
doxyfile = configure_file(input : 'Doxyfile.in',
                          output : 'Doxyfile',
                          configuration : config)
custom_target('doxygen',
              output : 'html',
              input : doxyfile,
              command : [doxygen, doxyfile])
