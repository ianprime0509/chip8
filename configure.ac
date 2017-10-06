dnl This seems like a safe minimum version, but could probably be made less
dnl conservative.
AC_PREREQ([2.69])
AC_INIT([chip8], [0.1.0], [ianprime0509@gmail.com])
AM_INIT_AUTOMAKE([-Wall -Werror foreign])
AC_CONFIG_HEADERS([config.h])
AC_CONFIG_MACRO_DIR([m4])

AC_PROG_CC
AC_PROG_CC_C99
# Try to use -Wall -Wextra
AX_CHECK_COMPILE_FLAG([-Wall], [AX_APPEND_FLAG([-Wall])])
AX_CHECK_COMPILE_FLAG([-Wextra], [AX_APPEND_FLAG([-Wextra])])
# Check for various functions
AC_CHECK_FUNCS(
    [clock_gettime,ffs,strdup],
    [],
    [AC_MSG_FAILURE([a required function was not found])]
)

SDL_VERSION=2.0.0
AM_PATH_SDL2(
    [$SDL_VERSION],
    [],
    [AC_MSG_FAILURE([could not find SDL version $SDL_VERSION])]
)

# Check for Doxygen
# Adapted from https://stackoverflow.com/a/42479100
AC_CHECK_PROGS([DOXYGEN], [doxygen])
if test -z "$DOXYGEN"; then
   AC_MSG_WARN([Doxygen not found; continuing without it])
fi
AM_CONDITIONAL([HAVE_DOXYGEN], [test -n "$DOXYGEN"])
AM_COND_IF([HAVE_DOXYGEN], [AC_CONFIG_FILES([docs/Doxyfile docs/Makefile])])
AC_CONFIG_FILES([Makefile src/Makefile])
AC_OUTPUT
