AC_PREREQ(2.53)
AC_INIT(psplash, 0.1, [http://bugzilla.o-hand.com/enter_bug.cgi?product=psplash])
AM_INIT_AUTOMAKE()
AC_CONFIG_SRCDIR(psplash.c)

AC_ISC_POSIX
AC_PROG_CC
AC_STDC_HEADERS

if test "x$GCC" = "xyes"; then
        GCC_FLAGS="-g -Wall -Wextra"
fi

DEFAULT_GFX_OUTPUT=fbdev

AC_ARG_WITH([gfx-output],
            AS_HELP_STRING([--with-gfx-output], [Specify the output technology stack: fbdev or drm [default=fbdev]]),
            [gfx_output=${withval}],
            [gfx_output=${DEFAULT_GFX_OUTPUT}])

case "${gfx_output}" in
  fbdev)
    AC_DEFINE([PSPLASH_FBDEV], [1], [Use fbdev as graphics output])
    ;;
  drm)
    AC_DEFINE([PSPLASH_DRM], [1], [Use DRM as graphics output])
    ;;
  *)
    AC_MSG_ERROR([Specify a valid graphics output technology with the --with-gfx-output option.  Valid values are fbdev and drm])
    ;;
esac

AM_CONDITIONAL([PSPLASH_FBDEV], [test x$gfx_output = xfbdev])
AM_CONDITIONAL([PSPLASH_DRM], [test x$gfx_output = xdrm])

AC_SUBST(GCC_FLAGS)

AC_OUTPUT([
Makefile
])
