desktopdir = $(datadir)/applications
dt_in_files = spacefm.desktop.in \
	       spacefm-find.desktop.in \
	       spacefm-folder-handler.desktop.in
desktop_DATA = $(dt_in_files:.desktop.in=.desktop)
@INTLTOOL_DESKTOP_RULE@

DISTCLEANFILES = spacefm.desktop \
		 spacefm-find.desktop \
		 spacefm-folder-handler.desktop

icondir = $(datadir)/pixmaps
icon_DATA = spacefm.png \
			spacefm-root.png \
			spacefm-find.png \
			spacefm-128-cube-blue.png \
			spacefm-128-cube-green.png \
			spacefm-128-cube-red.png \
			spacefm-128-pyramid-blue.png \
			spacefm-128-pyramid-green.png \
			spacefm-128-pyramid-red.png \
			spacefm-48-cube-blue.png \
			spacefm-48-cube-green.png \
			spacefm-48-cube-red.png \
			spacefm-48-folder-blue.png \
			spacefm-48-folder-red.png \
			spacefm-48-pyramid-blue.png \
			spacefm-48-pyramid-green.png \
			spacefm-48-pyramid-red.png

filedir = $(datadir)/spacefm
uidir=$(filedir)/ui

# sfm don't install mount.rules for no-hal build
if USE_HAL
file_DATA = \
	mount.rules
endif

# html-doc
htmldir = @htmldir@
html_DATA = spacefm-manual-en.html

# GtkBuilder UI definition files
ui_in_files= \
	ui/prefdlg.glade \
	ui/appchooserdlg.glade \
	ui/file_properties.glade \
	ui/about-dlg.glade \
	ui/find-files.glade \
	ui/godlg.glade
ui_DATA = $(ui_in_files:.glade=.ui)

EXTRA_DIST = \
	$(desktop_DATA) \
	$(icon_DATA) \
	$(file_DATA) \
	$(ui_DATA)
	
CLEANFILES = spacefm.desktop \
		 spacefm-find.desktop \
		 spacefm-folder-handler.desktop

# Generate GtkBuilder UI files from Glade files
%.ui: %.glade
#	$(GTK_BUILDER_CONVERT) $< $@
	$(top_srcdir)/gtk-builder-convert $< $@
#	$(top_builddir)/src/xml-purge $@

