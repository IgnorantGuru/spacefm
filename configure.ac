dnl Process this file with autoconf to produce a configure script.

AC_INIT([spacefm], [1.0.5])
AC_CONFIG_SRCDIR([src/main.c])
AM_INIT_AUTOMAKE([dist-bzip2] [subdir-objects])
AM_MAINTAINER_MODE

AM_DISABLE_STATIC

AC_ISC_POSIX
AC_PROG_CC
AM_PROG_LIBTOOL
AM_PROG_CC_STDC
AC_PROG_INTLTOOL
AC_HEADER_STDC

# sysconfdir defaults to /usr/local/etc if this is not done:
test "$sysconfdir" = '${prefix}/etc' && sysconfdir=/etc

# libtool option to control which symbols are exported
# right now, symbols starting with _ are not exported
LIBTOOL_EXPORT_OPTIONS='-export-symbols-regex "^[[^_]].*"'
AC_SUBST(LIBTOOL_EXPORT_OPTIONS)

# check for mmap()
AC_FUNC_MMAP

AC_CHECK_LIB(m, pow)
AC_CHECK_LIB(X11, XClearWindow)

AC_DEFINE(G_DISABLE_ASSERT, [1], [do not enable g_asserts])
AC_DEFINE(G_LOG_DOMAIN, "SpaceFM")

AC_ARG_WITH([bash-path], 
        AC_HELP_STRING([--with-bash-path=PATH], [Absolute path to GENUINE bash v4 - other shells lack features REQUIRED by SpaceFM (default: /bin/bash)]),
        AC_DEFINE_UNQUOTED(BASHPATH, "$withval", [path to bash]),
        AC_DEFINE_UNQUOTED(BASHPATH, "/bin/bash", [path to bash]))
bash_path="$withval"
# IMPORTANT: For security reasons, an absolute path must be used for genuine
# bash v4.  SpaceFM runs bash as root and you WILL open root exploits if you
# disable this check, or run bash based on $PATH.
if test ! -z "$bash_path"; then
    if test ! "${bash_path:0:1}" = "/"; then
        AC_MSG_ERROR([Fatal Error: Option --with-bash-path requires absolute path.])
    fi
    echo "Modifying spacefm-auth to use bash path..."
    bash_esc="${bash_path//\//\\/}"
    sed "s/\(\/bin\/bash\)/$bash_esc/" src/spacefm-auth.bash > src/spacefm-auth
else
    cp -pf src/spacefm-auth.bash src/spacefm-auth
fi

# gtk2 or gtk3
PKG_PROG_PKG_CONFIG
gtk2_modules="gtk+-2.0 >= 2.18.0 \
              gthread-2.0 \
              gobject-2.0"
gtk3_modules="gtk+-3.0 >= 3.0.0 \
              gthread-2.0 \
              gobject-2.0"
              
AC_ARG_WITH([gtk3], AS_HELP_STRING([--with-gtk3], [Build with GTK3 interface instead of GTK2 (default: with GTK2 unless GTK2 not installed)]), with_gtk3=$withval, with_gtk3="no")

AC_ARG_WITH([gtk2], AS_HELP_STRING([--with-gtk2], [Build with GTK2 interface only (default: with GTK2 unless GTK2 not installed)]), with_gtk2=$withval, with_gtk2="no")

if test x"$with_gtk3" = x"yes"; then
    # user enabled - gtk3 required
    PKG_CHECK_MODULES(GTK, [$gtk3_modules])
    AC_DEFINE([HAVE_GTK3], [1], [Use GTK3])
elif test x"$with_gtk2" = x"yes"; then
    # user enabled - gtk2 required
    PKG_CHECK_MODULES(GTK, [$gtk2_modules])
else
    # disabled - use gtk3 only if gtk2 not installed
    PKG_CHECK_MODULES(GTK, [$gtk2_modules], with_gtk3="no", [
       PKG_CHECK_MODULES(GTK, [$gtk3_modules], with_gtk3="yes", with_gtk3="no")
    ])
    if test x"$with_gtk3" = x"yes"; then
        AC_DEFINE([HAVE_GTK3], [1], [Use GTK3])
    else
        PKG_CHECK_MODULES(GTK, [$gtk2_modules])
    fi
fi
AC_SUBST([GTK_CFLAGS])
AC_SUBST([GTK_LIBS])

# Default make options
# disable deprecated warnings for now due to GTK2/GTK3 build compat
CPPFLAGS="$CPPFLAGS -Wno-deprecated-declarations -Wformat -Wformat-security -Wreturn-type -Wunused-value"

# start-notification is optional - will be used if installed and no --disable
AC_ARG_ENABLE(
    [startup-notification],
    AS_HELP_STRING([--disable-startup-notification],
                   [disable use of libstartup-notification (default: enable if installed)]),
    use_sn=$enableval, use_sn="yes")
if test x"$use_sn" = x"yes"; then
    sn_modules_old="libstartup-notification-0.9"
    sn_modules="libstartup-notification-1.0"
    PKG_CHECK_MODULES(SN, [$sn_modules_old], use_sn="yes", [
       PKG_CHECK_MODULES(SN, [$sn_modules], use_sn="yes", use_sn="no (not installed)")
    ])
    if test x"$use_sn" = x"yes"; then
        AC_SUBST(SN_CFLAGS)
        AC_SUBST(SN_LIBS)
        AC_DEFINE([HAVE_SN], [1], [Use SN])
    fi
fi

dnl linker optimizations
AC_MSG_CHECKING([whether $LD accepts --as-needed])
case `$LD --as-needed -v 2>&1 </dev/null` in
*GNU* | *'with BFD'*)
  LDFLAGS="$LDFLAGS -Wl,--as-needed"
  AC_MSG_RESULT([yes])
  ;;
*)
  AC_MSG_RESULT([no])
  ;;
esac
AC_MSG_CHECKING([whether $LD accepts -O1])
case `$LD -O1 -v 2>&1 </dev/null` in
*GNU* | *'with BFD'*)
  LDFLAGS="$LDFLAGS -Wl,-O1"
  AC_MSG_RESULT([yes])
  ;;
*)
  AC_MSG_RESULT([no])
  ;;
esac
AC_MSG_CHECKING([whether $LD accepts -Bsymbolic-functions])
case `$LD -Bsymbolic-functions -v 2>&1 </dev/null` in
*GNU* | *'with BFD'*)
  LDFLAGS="$LDFLAGS -Wl,-Bsymbolic-functions"
  AC_MSG_RESULT([yes])
  ;;
*)                                                                              
  AC_MSG_RESULT([no])
  ;;
esac

AC_ARG_ENABLE(
    [cast-checks],
    AS_HELP_STRING([--enable-cast-checks],
                   [enable Glib casting checks (default: disable)]),
    cast_checks=$enableval, cast_checks="no")
G_CAST_CHECKS=""
if test x"$cast_checks" = "xno"; then
    G_CAST_CHECKS="-DG_DISABLE_CAST_CHECKS"
fi
AC_SUBST(G_CAST_CHECKS)

AC_ARG_ENABLE(
    [hal],
    AS_HELP_STRING([--enable-hal],
                   [build with Linux HAL support (disables udisks) (default: no)]),
    use_hal=$enableval, use_hal="no")

if test x"$use_hal" = x"yes"; then
dnl Check HAL support
    hal_modules="dbus-glib-1 >= 0.31 hal >= 0.5.0 hal-storage"
    PKG_CHECK_MODULES(HAL, [$hal_modules], [have_hal=yes], [have_hal=no])

    if test x"$have_hal" = x"yes"; then
        AC_DEFINE([HAVE_HAL], [1], [Define to 1 if you have HAL.])
    else
        AC_MSG_ERROR([To use HAL support, you must have development packages of dbus-glib-1 (>=0.31), hal(>=0.5.0), and hal-storage, or you can use --disable-hal to disable HAL support.])
    fi
else
    PKG_CHECK_MODULES(LIBUDEV, [libudev >= 143])
    AC_SUBST(LIBUDEV_CFLAGS)
    AC_SUBST(LIBUDEV_LIBS)
fi
AM_CONDITIONAL(USE_HAL, test "$use_hal" = "yes")
AC_SUBST(HAL_CFLAGS)
AC_SUBST(HAL_LIBS)

AC_ARG_ENABLE([inotify],
    AS_HELP_STRING([--disable-inotify],
        [disable Linux inotify kernel support (requires fam/gamin instead) (default: enable)]),
    use_inotify=$enableval, use_inotify="yes")

if test x"$use_inotify" = x"yes"; then
    dnl Linux inotify is enabled.
    dnl So, both of FAM and gamin are not needed.
  AC_DEFINE(USE_INOTIFY, 1, [Whether to enable Linux inotify support])
else
    dnl ***************************
    dnl *** Check for Gamin/FAM ***
    dnl ***************************
    dnl *Modified from XFCE Thunar*

    FAM_CFLAGS=""
    FAM_LIBS=""
    have_libfam=no
    AC_CHECK_HEADERS([fam.h],
    [
        AC_CHECK_LIB([fam], [FAMOpen], [have_libfam="yes" FAM_LIBS="-lfam"])
    ])

    if test x"$have_libfam" = x"yes"; then
        dnl Define appropriate symbols
        AC_DEFINE([HAVE_FAM_H], [1], [Define to 1 if you have the <fam.h> header file.])
        AC_DEFINE([HAVE_LIBFAM], [1], [Define to 1 if the File Alteration Monitor is available.])

        dnl Check for FAMNoExists (currently Gamin only)
        save_LIBS="$LIBS"
        LIBS="$LIBS $FAM_LIBS"
        AC_CHECK_FUNCS([FAMNoExists])
        LIBS="$save_LIBS"
    else
        AC_MSG_ERROR([Fatal Error: no fam or gamin detected.])
    fi
fi
AC_SUBST([FAM_CFLAGS])
AC_SUBST([FAM_LIBS])

AC_ARG_ENABLE([largefile],
    AS_HELP_STRING([--enable-largefile],
        [enable Large file support (default: yes)]),
    largefile=$enableval, largefile="yes")

if test x"$largefile" = x"yes"; then
  CPPFLAGS="$CPPFLAGS -D_LARGEFILE_SOURCE -D_REENTRANT -D_FILE_OFFSET_BITS=64"
  AC_DEFINE(_LARGEFILE64_SOURCE, 1, [Whether to enable large file support])
  AC_DEFINE(_FILE_OFFSET_BITS, 64, [File offset bits])
fi

AC_ARG_ENABLE(
    [superuser-checks],
    AS_HELP_STRING([--disable-superuser-checks],
                   [disable checks running as super user (no current function) (default: enable)]),
    superuser_checks=$enableval, superuser_checks="yes")
if test x"$superuser_checks" = "xyes"; then
  AC_DEFINE(SUPER_USER_CHECKS, 1, [Whether to check running as super user or not])
fi

AC_ARG_WITH(
    [preferable-sudo],
    AS_HELP_STRING([--with-preferable-sudo=PROG],
                   [Specify custom graphical su program to run root commands (or see /etc/spacefm/spacefm.conf)]),
                   [ preferable_sudo="$withval" ])
if test ! -z "$preferable_sudo"; then
  CPPFLAGS="$CPPFLAGS -DPREFERABLE_SUDO_PROG=\\\"$preferable_sudo\\\""
fi

AC_ARG_ENABLE(
    [desktop-integration],
    AS_HELP_STRING([--disable-desktop-integration],
                   [disable desktop manager integration (default: enable)]),
    desktop_integration=$enableval, desktop_integration="yes")
if test x"$desktop_integration" = "xyes"; then
  AC_DEFINE(DESKTOP_INTEGRATION, 1, [Whether to integrate desktop or not.])
fi
AM_CONDITIONAL(DESKTOP_INTEGRATION, test "$desktop_integration" = "yes")

AC_ARG_ENABLE(
    [video-thumbnails],
    AS_HELP_STRING([--disable-video-thumbnails],
                   [disable libffmpegthumbnailer video thumbnails (default: enable)]),
    video_thumbnails=$enableval, video_thumbnails="yes")
if test x"$video_thumbnails" = "xyes"; then
    PKG_CHECK_MODULES( FFMPEG, libffmpegthumbnailer, HAVE_FFMPEG=yes)
    AC_SUBST(FFMPEG_CFLAGS)
    AC_SUBST(FFMPEG_LIBS)
    AC_DEFINE(HAVE_FFMPEG, 1, [Whether to use video thumbnails or not.])
fi

AC_ARG_ENABLE(
    [pixmaps],
    AS_HELP_STRING([--enable-pixmaps],
        [use share/pixmaps dir instead of share/icons dir to store icons (default: disable)]),
    use_pixmaps=$enableval, use_pixmaps="no")
AM_CONDITIONAL(NO_PIXMAPS, test ! x"$use_pixmaps" = x"yes")

AC_ARG_ENABLE(
    [btrfs-clone],
    AS_HELP_STRING([--disable-btrfs-clone],
        [try to use btrfs clone ioctl to copy files (default: enable if installed)]),
    use_btrfs_clone=$enableval, use_btrfs_clone="yes")
if test x"$use_btrfs_clone" = x"yes"; then
  AC_CHECK_HEADERS([sys/ioctl.h btrfs/ioctl.h], use_btrfs_clone="yes", use_btrfs_clone="no")
  if test x"$use_btrfs_clone" = x"yes"; then
    AC_DEFINE(HAVE_BTRFS_CLONE, 1, [Use Btrfs clone])
  fi
fi

dnl advanced compiler tweaking
CPPFLAGS="$CPPFLAGS -fstrict-aliasing -fmessage-length=0"

dnl data dirs
CPPFLAGS="$CPPFLAGS -DDATADIR=\\\"$datadir\\\" -DHTMLDIR=\\\"$htmldir\\\""

AC_CHECK_FUNC(euidaccess,[AC_DEFINE(HAVE_EUIDACCESS,[],[Define to 1 if euidaccess is available])])
AC_CHECK_FUNC(eaccess,[AC_DEFINE(HAVE_EACCESS,[],[Define to 1 if eaccess is available])])

AC_CHECK_FUNC(statvfs,[AC_DEFINE(HAVE_STATVFS,[],[Define to 1 if statvfs is available])])


# Gtk Builder
#AC_PATH_PROG([GTK_BUILDER_CONVERT],[gtk-builder-convert],[false])
#if test "$GTK_BUILDER_CONVERT" = "false"; then
#  AC_MSG_ERROR([gtk-builder-convert not found])
#fi

GETTEXT_PACKAGE=spacefm
AC_SUBST(GETTEXT_PACKAGE)
AC_DEFINE_UNQUOTED(GETTEXT_PACKAGE,"$GETTEXT_PACKAGE", [Gettext package.])

dnl Add the languages which your application supports here.
ALL_LINGUAS="af ar da ca cs de es et eu fa fi fr gl he hr hu id it ja ko lt ml ms nb nl nn pl ps pt pt_BR ru sk sl sv tr uk ur ur_PK vi zh_CN zh_TW"
AM_GLIB_GNU_GETTEXT

AC_OUTPUT([
Makefile
etc/Makefile
src/Makefile
po/Makefile.in
data/Makefile
])

resolve_datadir="$(eval echo "$datadir")"
while [[ "${resolve_datadir:0:1}" == "$" ]]; do
    resolve_datadir="$(eval echo "$resolve_datadir")"
done

resolve_htmldir="$(eval echo "$htmldir")"
while [[ "${resolve_htmldir:0:1}" == "$" ]]; do
    resolve_htmldir="$(eval echo "$resolve_htmldir")"
done

echo
echo SpaceFM...................................... : Version $VERSION
echo
echo Prefix....................................... : $prefix
echo Executable................................... : $prefix/bin/spacefm
echo Configuration Dir............................ : "$sysconfdir/spacefm/"
if test x"$use_pixmaps" = x"yes"; then
echo Icon Dir..................................... : "$resolve_datadir/pixmaps/"
else
echo Icon Dir..................................... : "$resolve_datadir/icons/hicolor/"
fi
echo Documentation Dir............................ : "$resolve_htmldir/"
if test ! -z "$bash_path"; then
echo Custom path to bash v4....................... : "$bash_path"
fi
if test x"$use_hal" = x"yes"; then
echo Linux device support......................... : hal
else
echo Linux device support......................... : libudev
fi
if test x"$use_inotify" = x"yes"; then
echo Linux file change monitor support............ : inotify
else
echo Linux file change monitor support............ : fam/gamin
fi
echo 64-bit Large file support.................... : $largefile
#echo Show warnings if run as super user........... : $superuser_checks
if test ! -z "$preferable_sudo"; then
echo Additional graphical su program.............. : $preferable_sudo
fi
if test x"$with_gtk3" = x"yes"; then
echo 'GTK+ Version................................. : GTK 3'
else
echo 'GTK+ Version................................. : GTK 2'
fi
echo Desktop manager integration.................. : $desktop_integration
echo Startup notification......................... : $use_sn
echo Video thumbnail support...................... : $video_thumbnails
echo Btrfs clone support.......................... : $use_btrfs_clone
echo
echo 'Homepage: http://ignorantguru.github.io/spacefm/'
echo
echo Please read README carefully if you are packaging SpaceFM.
echo
if test ! x"$use_pixmaps" = x"yes"; then
echo
echo 'IMPORTANT: After make and install, run:'
echo "           sudo gtk-update-icon-cache -q -t -f $resolve_datadir/icons/hicolor"
echo "           sudo gtk-update-icon-cache -q -t -f $resolve_datadir/icons/Faenza"
echo
fi
if test ! -z "$bash_path"; then
echo "IMPORTANT: SpaceFM is highly integrated with bash and uses features of"
echo "           bash v4.  Attempting to substitute another shell for bash WILL cause"
echo "           feature breakage, even if not immediately apparent.  Use genuine"
echo "           bash."
echo
fi
echo
