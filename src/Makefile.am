## Process this file with automake to produce Makefile.in

INCLUDES = \
    -I$(top_srcdir)/src/mime-type \
    -I$(top_srcdir)/src/compat \
    -I$(top_srcdir)/src/ptk \
    -I$(top_srcdir)/src/vfs \
    -I$(top_srcdir)/src/libmd5-rfc \
    -I$(top_srcdir)/src/desktop \
    -I$(top_srcdir)/src/exo \
    -DPACKAGE_DATA_DIR=\""$(datadir)/spacefm"\" \
    -DPACKAGE_UI_DIR=\""$(datadir)/spacefm/ui"\" \
    -DPACKAGE_LOCALE_DIR=\""$(prefix)/$(DATADIRNAME)/locale"\"

bin_PROGRAMS = spacefm

bin_SCRIPTS = spacefm-auth

MIMETYE_SOURCES = \
    mime-type/mime-type.c mime-type/mime-type.h \
    mime-type/mime-action.c mime-type/mime-action.h \
    mime-type/mime-cache.c mime-type/mime-cache.h

MD5_SOURCES = \
    libmd5-rfc/md5.c libmd5-rfc/md5.h

VOLUME_HAL_SOURCES= \
    vfs/vfs-volume-hal.c \
    vfs/vfs-volume-hal-options.h \
    vfs/vfs-volume-hal-options.c

VOLUME_NOHAL_SOURCES= \
    vfs/vfs-volume-nohal.c

if USE_HAL
VOLUME_SOURCES= \
    $(VOLUME_HAL_SOURCES)
else
VOLUME_SOURCES= \
    $(VOLUME_NOHAL_SOURCES)
endif

VFS_SOURCES = \
    vfs/vfs-file-monitor.c vfs/vfs-file-monitor.h \
    vfs/vfs-mime-type.c vfs/vfs-mime-type.h \
    vfs/vfs-file-info.c vfs/vfs-file-info.h \
    vfs/vfs-dir.c vfs/vfs-dir.h \
    vfs/vfs-app-desktop.c vfs/vfs-app-desktop.h \
    vfs/vfs-file-task.c vfs/vfs-file-task.h \
    vfs/vfs-volume.h \
    $(VOLUME_SOURCES) \
    vfs/vfs-execute.c vfs/vfs-execute.h \
    vfs/vfs-async-task.c vfs/vfs-async-task.h \
    vfs/vfs-thumbnail-loader.c vfs/vfs-thumbnail-loader.h \
    vfs/vfs-utils.c vfs/vfs-utils.h

if DESKTOP_INTEGRATION
DESKTOP_SOURCES = \
    desktop/desktop-window.c desktop/desktop-window.h \
    desktop/desktop.c desktop/desktop.h
else
DESKTOP_SOURCES = desktop/desktop.c desktop/desktop.h
endif

PTK_SOURCES = \
    ptk/ptk-file-browser.c ptk/ptk-file-browser.h \
    ptk/ptk-file-list.c ptk/ptk-file-list.h \
    ptk/ptk-path-entry.c ptk/ptk-path-entry.h \
    ptk/ptk-text-renderer.c ptk/ptk-text-renderer.h \
    ptk/ptk-file-icon-renderer.c ptk/ptk-file-icon-renderer.h \
    ptk/ptk-utils.c ptk/ptk-utils.h \
    desktop/working-area.c desktop/working-area.h \
    ptk/ptk-dir-tree-view.c ptk/ptk-dir-tree-view.h \
    ptk/ptk-dir-tree.c ptk/ptk-dir-tree.h \
    ptk/ptk-location-view.c ptk/ptk-location-view.h \
    ptk/ptk-bookmarks.c ptk/ptk-bookmarks.h \
    ptk/ptk-input-dialog.c ptk/ptk-input-dialog.h \
    ptk/ptk-file-task.c ptk/ptk-file-task.h \
    ptk/ptk-file-archiver.c ptk/ptk-file-archiver.h \
    ptk/ptk-handler.c ptk/ptk-handler.h \
    ptk/ptk-clipboard.c ptk/ptk-clipboard.h \
    ptk/ptk-file-menu.c ptk/ptk-file-menu.h \
    ptk/ptk-file-misc.c ptk/ptk-file-misc.h \
    ptk/ptk-file-properties.c ptk/ptk-file-properties.h \
    ptk/ptk-app-chooser.c ptk/ptk-app-chooser.h

EXO_SOURCES = \
    exo/exo-icon-view.c exo/exo-icon-view.h \
    exo/exo-tree-view.c exo/exo-tree-view.h \
    exo/exo-private.c exo/exo-private.h \
    exo/exo-string.c exo/exo-string.h \
    exo/exo-marshal.c exo/exo-marshal.h

COMPAT_SOURCES = \
    compat/glib-mem.h \
    compat/glib-utils.h \
    compat/glib-utils.c

spacefm_SOURCES = \
    $(MIMETYE_SOURCES) \
    $(VFS_SOURCES) \
    $(MD5_SOURCES) \
    $(COMPAT_SOURCES) \
    $(PTK_SOURCES) \
    $(EXO_SOURCES) \
    $(DESKTOP_SOURCES) \
    private.h \
    main.c pcmanfm.h \
    main-window.c main-window.h \
    settings.c settings.h \
    pref-dialog.c pref-dialog.h \
    find-files.c find-files.h \
    go-dialog.c go-dialog.h \
    cust-dialog.c cust-dialog.h \
    item-prop.c item-prop.h

spacefm_CFLAGS = \
    @GTK_CFLAGS@ \
    @SN_CFLAGS@ \
    @HAL_CFLAGS@ \
    @LIBUDEV_CFLAGS@ \
    @G_CAST_CHECKS@ \
    -Werror-implicit-function-declaration

spacefm_LDADD = \
    @GTK_LIBS@ \
    $(INTLLIBS) \
    @FAM_LIBS@ \
    @HAL_LIBS@ \
    @SN_LIBS@ \
    @LIBUDEV_LIBS@

noinst_PROGRAMS=xml-purge
xml_purge_SOURCES=xml-purge.c
xml_purge_CFLAGS=@GTK_CFLAGS@
xml_purge_LDADD=@GTK_LIBS@


