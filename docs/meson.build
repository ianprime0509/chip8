doxygen = find_program('doxygen')
doxyfile = configure_file(input : 'Doxyfile.in',
                          output : 'Doxyfile',
                          configuration : config,
                          install : false)
custom_target('doxygen',
              output : 'html',
              input : doxyfile,
              command : [doxygen, doxyfile],
              install : false)
